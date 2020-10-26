#pragma once

#include "core\memory\allocator.h"

struct LinearAllocator : Allocator {
	size_t occupied;
	size_t max_size;

	char* memory;
	Allocator* parent;

	LinearAllocator(const LinearAllocator&) = delete;
	
	inline void operator=(LinearAllocator&& allocator) {
		occupied = allocator.occupied;
		max_size = allocator.max_size;
		memory = allocator.memory;
		parent = allocator.parent;

		allocator.occupied = 0;
		allocator.max_size = 0;
		allocator.memory = nullptr;
	}

	inline LinearAllocator() {
		occupied = 0;
		max_size = 0;
		memory = nullptr;
		parent = nullptr;
	}

	inline LinearAllocator(size_t max_size, Allocator* parent = &default_allocator) {
		this->parent = parent;
		this->occupied = 0;
		this->max_size = max_size;
		this->memory = (char*)parent->allocate(max_size);
	}

	inline ~LinearAllocator() {
		if (parent) parent->deallocate(memory);
	}

	inline void* allocate(size_t size) final {
		u64 offset = aligned_incr(&occupied, size, 16);

		if (occupied > max_size) {
			throw "Temporary allocator out of memory";
		}

		return memory + offset;
	}

	inline void reset(size_t occupied) {
		this->occupied = occupied;
	}

	inline void clear() {
		this->occupied = 0;
	}
};

struct LinearRegion {
	LinearAllocator& allocator;
	size_t watermark;

	inline LinearRegion(LinearAllocator& allocator) : allocator(allocator) {
		watermark = allocator.occupied;
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
#define TEMPORARY_ZEROED_ARRAY(name, num) new (get_temporary_allocator().allocate(sizeof(name) * num)) name[num]{0}

#define PERMANENT_ALLOC(name, ...) new (get_permanent_allocator().allocate(sizeof(name))) name(__VA_ARGS__)
#define PERMANENT_ARRAY(name, num) new (get_permanent_allocator().allocate(sizeof(name) * num)) name[num]