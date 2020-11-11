#pragma once

#include "core/core.h"

struct Fiber;
struct FLS;

Fiber* make_fiber(u64 stack_size, void(*func)());
Fiber* get_current_fiber();
Fiber* convert_thread_to_fiber();
void convert_fiber_to_thread();
void switch_to_fiber(Fiber*);
void free_fiber(Fiber*);

FLS* make_FLS(void*);
void free_FLS(FLS*);

bool set_FLS(FLS*, void*);
void* get_FLS(FLS*);
