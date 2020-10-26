#pragma once

#include <cstddef>
#include "core/core.h"

struct CORE_API Allocator {
	virtual void* allocate(std::size_t) { return NULL;  };
	virtual void deallocate(void* ptr) {};
};

struct CORE_API BlockAllocator : Allocator {
	static constexpr int block_size = 16000;
	static constexpr int num_blocks = 20;

	struct Block {
		Block* next = NULL;
		char data[block_size];
		size_t offset = 0;
		uint count = 0;
	};
	
	Block blocks[num_blocks];

	Block* free_block = NULL;

	BlockAllocator(const BlockAllocator &) = delete;

	void* allocate(std::size_t);
	void deallocate(void* ptr);

	void next_empty_block();

	BlockAllocator();
};

struct CORE_API MallocAllocator : Allocator {
	void* allocate(std::size_t);
	void deallocate(void* ptr);
};

extern CORE_API BlockAllocator block_allocator;
extern CORE_API MallocAllocator default_allocator;

#define ALLOC(T, ...) new (default_allocator.allocate(sizeof(T)) T((__VA_ARGS__)

template<typename T>
inline void FREE(T* ptr) {
	if (ptr == NULL) return;
	ptr.~T();
	default_allocator.deallocate(ptr);
}

#define ALLOC_ARRAY(T, N) new (default_allocator.allocate(sizeof(T) * N)) T[N]

template<typename T>
inline void FREE_ARRAY(T* ptr, std::size_t N) {
	if (ptr == NULL) return;
	for (int i = 0; i < N; i++) {
		ptr[i].~T();
	}

	default_allocator.deallocate(ptr);
}

template<typename T>
void destruct(T* ptr) {
	ptr->~T();
}

template<typename T>
void memcpy_t(T* a, T* b, u64 count) {
	memcpy(a, b, count * sizeof(T));
}

inline u64 align_offset(u64 offset, uint alignment) {
	uint alignment_bytes_minus_one = alignment - 1;
	return (offset + alignment_bytes_minus_one) & ~alignment_bytes_minus_one;
}

inline u64 aligned_incr(u64* occupied, uint size, uint alignment) {
	u64 offset = align_offset(*occupied, alignment);
	*occupied = offset + size;
	return offset;
}

CORE_API Allocator& get_allocator();