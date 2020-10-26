#pragma once

#include "core/core.h"
#include "core/container/slice.h"
#include "core/job_system/thread.h"
#include <atomic>

using JobFunc = void(*)(void*);

enum Priority {
	PRIORITY_HIGH,
	PRIORITY_MEDIUM,
	PRIORITY_LOW,
	PRIORITY_COUNT
};

struct JobDesc {
	JobFunc func;
	void* data;

	JobDesc() : func(nullptr), data(nullptr) {}
	JobDesc(JobFunc func, void* data) : func(func), data(data) {}

	template<typename T>
	JobDesc(void(*func)(T&), T* data) : func((JobFunc)func), data(data) {}
};

using atomic_counter = std::atomic<uint>;

struct WaitCondition {
	void* fiber;
	atomic_counter counter;
	uint value;

	CORE_API WaitCondition();
};

CORE_API void make_job_system(uint max_fibers, uint num_workers = hardware_thread_count());
CORE_API void destroy_job_system();
CORE_API void add_jobs(Priority, slice<JobDesc>, atomic_counter*);
CORE_API void schedule_jobs_on(slice<uint>, slice<JobDesc>, atomic_counter*);
CORE_API void wait_for_jobs(Priority, slice<JobDesc>);
CORE_API void wait_for_counter(atomic_counter*, uint value);
CORE_API void wait_for_jobs_on_thread(Priority, slice<JobDesc>);
CORE_API void wait_for_counter_on_thread(atomic_counter*, uint value);

//ENGINE_API void wait_for_counter_on_thread(atomic_counter*, uint value = 0);

//#define JOB_ENTRY_POINT(name, type, variable) void name(void* ) 