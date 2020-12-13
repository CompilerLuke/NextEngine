#pragma once

#include "core/core.h"

struct Fiber;
struct FLS;

CORE_API Fiber* make_fiber(u64 stack_size, void(*func)(void*));
CORE_API Fiber* get_current_fiber();
CORE_API Fiber* convert_thread_to_fiber();
CORE_API void convert_fiber_to_thread();
CORE_API void switch_to_fiber(Fiber*);
CORE_API void free_fiber(Fiber*);

CORE_API FLS* make_FLS(void*);
CORE_API void free_FLS(FLS*);

CORE_API bool set_FLS(FLS*, void*);
CORE_API void* get_FLS(FLS*);
