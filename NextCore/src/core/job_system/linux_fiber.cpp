#ifdef NE_PLATFORM_MACOSX
#include "core/job_system/fiber.h"
#include <stdlib.h>

#define _XOPEN_SOURCE
#include <ucontext.h>

#include "core/memory/allocator.h"
#include <stdio.h>

struct Fiber;
static thread_local Fiber* currently_executing_fiber = nullptr;

Fiber* make_fiber(u64 stack_size, void(*func)(void*)) {
	/* Create a context */
    
    u64 size = sizeof(ucontext_t);
    u64 stack_offset = aligned_incr(&size, stack_size, 64);

	char* memory = (char*)malloc(size);
    
    ucontext_t* ctx = (ucontext_t*)memory;
    
	getcontext(ctx);
    ctx->uc_stack.ss_sp = memory + stack_offset;
	ctx->uc_stack.ss_size = stack_size;
	ctx->uc_link = 0;
	makecontext(ctx, (void(*)())func, 0);

	return (Fiber*)ctx;
}

Fiber* get_current_fiber() {
	return currently_executing_fiber;
}

Fiber* convert_thread_to_fiber() {
	currently_executing_fiber = (Fiber*)malloc(sizeof(ucontext_t) * 5);
	getcontext((ucontext_t*)currently_executing_fiber);

	return currently_executing_fiber;
}

void convert_fiber_to_thread() {
    free_fiber(currently_executing_fiber);
}

void switch_to_fiber(Fiber* fiber) {
	Fiber* current = currently_executing_fiber;
	currently_executing_fiber = fiber;
    
    //printf("Yield to fiber with %lu\n", ((ucontext_t*)fiber)->uc_stack.ss_size);
	swapcontext((ucontext_t*)current, (ucontext_t*)fiber);
}

void free_fiber(Fiber* fiber) {
	ucontext_t* ctx = (ucontext_t*)fiber;
    printf("Freeing fiber!!!\n");
    free(fiber);
}


FLS* make_FLS(void* ptr) {
	return NULL;
}

void* get_FLS(FLS* fls) {
	return NULL;
}

bool set_FLS(FLS* fls, void* value) {
	return false;
}

void free_FLS(FLS* fls) {

}
#endif
