#pragma once

#include "core\memory\allocator.h"

struct LinearAllocator : Allocator {
	size_t occupied;
	size_t max_size;

	char* memory;

	LinearAllocator(const LinearAllocator&) = delete;

	ENGINE_API LinearAllocator(size_t);
	ENGINE_API ~LinearAllocator();

	ENGINE_API void* allocate(size_t) final override;
	ENGINE_API void reset(size_t occupied);
	ENGINE_API void clear();
};

struct LinearRegion {
	LinearAllocator& allocator;
	size_t watermark;

	LinearRegion(LinearAllocator& allocator);
	~LinearRegion();
};

extern ENGINE_API LinearAllocator temporary_allocator;
extern ENGINE_API LinearAllocator permanent_allocator;

#define TEMPORARY_ALLOC(name, ...) new (temporary_allocator.allocate(sizeof(name))) name(__VA_ARGS__)
#define TEMPORARY_ARRAY(name, num) new (temporary_allocator.allocate(sizeof(name) * num)) name[num]

#define PERMANENT_ALLOC(name, ...) new (permanent_allocator.allocate(sizeof(name))) name(__VA_ARGS__)
#define PERMANENT_ARRAY(name, num) new (permanent_allocator.allocate(sizeof(name) * num)) name[num]