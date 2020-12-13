#ifdef NE_PLATFORM_WINDOWS
#include "core/job_system/fiber.h"
#include <stdlib.h>
#include <Windows.h>

struct Fiber;

//FIBER API is closely modelled after the Windows API and translates to direct function calls
Fiber* make_fiber(u64 stack_size, void(*func)(void*)) {
	return (Fiber*)CreateFiber(stack_size, func, nullptr);
}

Fiber* get_current_fiber() {
	return (Fiber*)GetCurrentFiber();
}

Fiber* convert_thread_to_fiber() {
	return (Fiber*)ConvertThreadToFiber(nullptr);
}

void convert_fiber_to_thread() {
	ConvertFiberToThread();
}

void switch_to_fiber(Fiber* fiber) {
	SwitchToFiber(fiber);
}

void free_fiber(Fiber* fiber) {
	DeleteFiber(fiber);
}

FLS* make_FLS(void* ptr) {
	FLS* fls = (FLS*)(size_t)FlsAlloc(nullptr);
	set_FLS(fls, ptr);
	return fls;
}

void* get_FLS(FLS* fls) {
	return FlsGetValue((DWORD)fls);
}

bool set_FLS(FLS* fls, void* value) {
	return FlsSetValue((DWORD)fls, value);
}

void free_FLS(FLS* fls) {
	FlsFree((DWORD)fls);
}
#endif