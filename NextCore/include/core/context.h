#pragma once

#include "memory/allocator.h"

struct LinearAllocator;
struct Allocator;
struct Logger;

struct Context {
	LinearAllocator* temporary_allocator = nullptr;
	Allocator* allocator = &default_allocator;
};

struct ScopedContext {
	Context* previus;
	Context current;

	CORE_API ScopedContext(Context&);
	CORE_API ~ScopedContext();
};

CORE_API Context& get_context();
CORE_API void set_context(Context*);
