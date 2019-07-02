#pragma once

#include <cstdlib>
#include "allocator.h"

struct TemporaryAllocator : Allocator {
	size_t occupied;
	size_t max_size;

	char* memory;

	TemporaryAllocator(const TemporaryAllocator&) = delete;

	TemporaryAllocator(size_t);
	~TemporaryAllocator();

	void* allocate(size_t) override;
	void clear();
};

extern TemporaryAllocator temporary_allocator;

#define TEMPORARY_ALLOC(name, ...) new (temporary_allocator.allocate(sizeof(name))) name(__VA_ARGS__)
#define TEMPORARY_ARRAY(name, num) new (temporary_allocator.allocate(sizeof(name) * num)) name[num]

template<typename T>
struct STDTemporaryAllocator : public std::allocator<T> {
	T* allocate(std::size_t size) {
		return temporary_allocator->alloc(size);
	}

	void deallocate(T* ptr, std::size_t) {};
};
