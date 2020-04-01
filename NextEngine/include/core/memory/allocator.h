#pragma once

#include <cstddef>
#include "core/core.h"

struct ENGINE_API Allocator {
	virtual void* allocate(std::size_t) { return NULL;  };
	virtual void deallocate(void* ptr) {};
};

struct ENGINE_API BlockAllocator : Allocator {
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

struct ENGINE_API MallocAllocator : Allocator {
	void* allocate(std::size_t);
	void deallocate(void* ptr);
};

extern ENGINE_API BlockAllocator block_allocator;
extern ENGINE_API MallocAllocator default_allocator;

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