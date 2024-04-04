#pragma once


#include "core/memory/allocator.h"
#include <stdio.h>
#include <new>
#include <stdlib.h>

struct LinearAllocator : Allocator {
private:
    struct BlockHeader {
        BlockHeader* prev = nullptr;
        BlockHeader* next = nullptr;
        size_t occupied = 0;
        size_t size = 0;
    };

    size_t default_block_size = 0;
    BlockHeader* head = nullptr;
    Allocator* parent = nullptr;
    uint64_t occupied = 0;

public:
    LinearAllocator(const LinearAllocator&) = delete;

    uint64_t get_occupied() const {return occupied;}

    inline void push_block(u64 block_size) {
        assert(block_size >= sizeof(BlockHeader));

        //if(head) occupied += head->size - head->occupied;
        if(head && head->next && head->next->size >= block_size) { // Empty block is already allocated
            head = head->next;
            assert(head->occupied == 0);
            //printf("PUSH BLOCK allocator: sufficient block found\n");
            return;
        }

        if(parent == nullptr) parent = &default_allocator;

        BlockHeader* new_head = (BlockHeader*)parent->allocate(block_size);
        *new_head = {};
        new_head->prev = head;
        new_head->next = head ? head->next : nullptr;
        new_head->occupied = 0;
        new_head->size = block_size - sizeof(BlockHeader);
        if(head) {
            if(head->next) head->next->prev = new_head;
            head->next = new_head;
        }
        head = new_head;
        //printf("PUSH BLOCK allocator: new block inserted %lu\n", block_size);
    }

    inline LinearAllocator() {}
    inline LinearAllocator(size_t default_block_size, Allocator* parent = &default_allocator) {
        this->default_block_size = default_block_size;
        this->parent = parent;
    }
    inline LinearAllocator(LinearAllocator&& other) {
        default_block_size = other.default_block_size;
        head = other.head;
        parent = other.parent;
        occupied = other.occupied;
        other.head = nullptr;
        other.occupied = 0;
        // printf("MOVE CONSTRUCTOR allocator\n");
    }
    inline void operator=(LinearAllocator&& other) {
        this->~LinearAllocator();
        default_block_size = other.default_block_size;
        head = other.head;
        parent = other.parent;
        occupied = other.occupied;
        other.head = nullptr;
        other.occupied = 0;
        // printf("MOVE ASSIGN allocator\n");
    }

    inline ~LinearAllocator() {
        if (parent && head) {
            BlockHeader* curr = head;
            while(curr->prev) curr = curr->prev;
            while(curr) {
                BlockHeader* next = curr->next;
                parent->deallocate(curr);
                curr = next;
            }
        }
    }

#if 1
    inline void check_alloc(const char* msg) {
        BlockHeader* curr = this->head;
        size_t total_occupied = 0;
        while(curr && curr->next) curr = curr->next;
        while (curr) {
            total_occupied += curr->occupied;
            if(curr->next) assert(curr->next->prev == curr);
            curr = curr->prev;
        }

        if (total_occupied != occupied) {
            printf("Wrong occupied @ %s", msg);
            exit(1);
        }
    }
#else
    inline void check_alloc(const char* msg) {}
#endif

    inline void* allocate(size_t size) final {
        check_alloc("BEGIN of allocate");
        // todo: could cache occupied

        if(!head) {
            default_block_size = default_block_size==0 ? kb(10) : default_block_size;
            push_block(max(size + sizeof(BlockHeader), default_block_size));
        }

        u64 prev_occupied = head->occupied;
        u64 offset = align_offset(prev_occupied, 16);
        if (offset+size > head->size) {
            if((int64_t)size > (int64_t)default_block_size-(int64_t)sizeof(BlockHeader)) {
                WARN("Allocated %lu bytes, which is larger than block size %lu\n", size_t(size), size_t(head->size));
                push_block(size + sizeof(BlockHeader));
                check_alloc("BIG block");
            } else {
                push_block(default_block_size);
                check_alloc("NEW block");
            }
            assert(head->occupied <= head->size);
            head->occupied = size;
            occupied += size;
            offset = 0;
            check_alloc("END of allocate");
        } else {
            head->occupied = offset + size;
            occupied += head->occupied - prev_occupied;
            check_alloc("INSIDE block");
        }

        //printf("Allocated %ul\n", size);

        return (char*)(head + 1) + offset;
    }

    inline void reset(size_t occupied) {
        if(!head) return;

        assert(occupied <= this->occupied);

        size_t shrink = this->occupied - occupied;

        while(head && shrink > head->occupied) {
            shrink -= head->occupied;
            bool big_block = head->size > default_block_size - sizeof(BlockHeader);
            if(big_block) {
                BlockHeader* prev = head->prev;

                head->prev->next = head->next; // Remove block
                head->next->prev = head->prev;

                if(parent) parent->deallocate(head);
                else printf("WARN - leaking big block memory no parent allocator");

                head = prev;
            } else {
                head->occupied = 0;
                head = head->prev;
            }
        }
        if (head) {
            head->occupied -= shrink;
            this->occupied = occupied;
            //printf("Reset %ul\n", occupied);
            check_alloc("RESET head");
        } else {
            printf("Empty head");
            exit(1);
        }
    }

    inline void clear() {
        reset(0);
    }
};

struct LinearRegion {
    LinearAllocator& allocator;
    size_t watermark;

    inline LinearRegion(LinearAllocator& allocator) : allocator(allocator) {
        watermark = allocator.get_occupied();
    }

    inline ~LinearRegion() {
        allocator.reset(watermark);
    }
};

CORE_API LinearAllocator& get_temporary_allocator();
CORE_API LinearAllocator& get_permanent_allocator();
CORE_API LinearAllocator& get_thread_local_temporary_allocator();
CORE_API LinearAllocator& get_thread_local_permanent_allocator();

#define TEMPORARY_ALLOC(name, ...) new (get_temporary_allocator().allocate(sizeof(name))) name(__VA_ARGS__)
#define TEMPORARY_ARRAY(name, num) new (get_temporary_allocator().allocate(sizeof(name) * num)) name[num]
#define TEMPORARY_ZEROED_ARRAY(name, num) new (get_temporary_allocator().allocate(sizeof(name) * num)) name[num]()

#define PERMANENT_ALLOC(name, ...) new (get_permanent_allocator().allocate(sizeof(name))) name(__VA_ARGS__)
#define PERMANENT_ARRAY(name, num) new (get_permanent_allocator().allocate(sizeof(name) * num)) name[num]

