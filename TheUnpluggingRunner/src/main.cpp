#include "stdafx.h"
#define PROFILING

#include <engine/application.h>
#include <engine/engine.h>
#include <core/memory/linear_allocator.h>
#include <core/memory/allocator.h>
#include <core/time.h>
#include <core/job_system/job.h>
#include <core/job_system/fiber.h>
#include <core/context.h>
#include <stdio.h>

atomic_counter counter;

void nop(void* data) {
	(counter += 1);
	//printf("Nop %i\n", current);
}

void fork3(void* data) {
	//u64 index = (u64)data;

	const uint JOB_COUNT = 10;

	JobDesc desc[JOB_COUNT] = {};
	for (uint i = 0; i < JOB_COUNT; i++) desc[i] = { nop, nullptr };

	wait_for_jobs(PRIORITY_HIGH, { desc, JOB_COUNT });
}

void fork2(void* data) {
	//u64 index = (u64)data;

	const uint JOB_COUNT = 10;

	JobDesc desc[JOB_COUNT] = {};
	for (uint i = 0; i < JOB_COUNT; i++) desc[i] = { fork3, nullptr };

	wait_for_jobs(PRIORITY_HIGH, { desc, JOB_COUNT });
}

void fork(void* data) {
	//u64 index = (u64)data;

	const uint JOB_COUNT = 10;

	JobDesc desc[JOB_COUNT] = {};
	for (uint i = 0; i < JOB_COUNT; i++) desc[i] = {fork2, nullptr };

	wait_for_jobs(PRIORITY_HIGH, { desc, JOB_COUNT });
}

void job_system_test(void*) {
	double start_time = Time::now();

	const uint JOB_COUNT = 65;

	JobDesc* desc = PERMANENT_ARRAY(JobDesc, JOB_COUNT);
	for (uint i = 0; i < JOB_COUNT; i++) {
		desc[i] = { fork, (void*)(u64)i };
	}

	for (uint i = 0; i < 1; i++) {
		wait_for_jobs(PRIORITY_HIGH, { desc, JOB_COUNT });
	}

	double end_time = Time::now();
	printf("Took %f ms\n", (end_time - start_time) * 1000);
	//printf("Executed %i jobs", (uint)counter);
}

void job_system_test_main() {
	make_job_system(200, 8);

	JobDesc desc{job_system_test, nullptr};
	wait_for_jobs_on_thread(PRIORITY_HIGH, desc);

	destroy_job_system(); 
}

#define BUILD_STANDALONE

void init_workers(void*) {
    printf("Initialized worker %i\n", get_worker_id());
    get_thread_local_permanent_allocator() = LinearAllocator(mb(10));
    get_thread_local_temporary_allocator() = LinearAllocator(mb(100));

    Context& ctx = get_context();
    ctx.allocator = &default_allocator;
    ctx.temporary_allocator = &get_thread_local_temporary_allocator();

    //printf("Linear allocator %p on fiber %i\n", &get_thread_local_temporary_allocator(), get_worker_id());
}

void fiber_main(void* data) {
	LinearAllocator& permanent_allocator = get_thread_local_permanent_allocator(); 
	LinearAllocator& temporary_allocator = get_thread_local_temporary_allocator();

	permanent_allocator = LinearAllocator(mb(200));
	temporary_allocator = LinearAllocator(mb(200));

	Context& context = get_context();
	context.temporary_allocator = &get_thread_local_temporary_allocator();
	context.allocator = &default_allocator;

	const char* level = "CFD/data/test1/";
    const char* engine_asset_path = "NextEngine/data/";
	const char* app_name = "CFD";
    
    printf("Initializing on worker %i\n", get_worker_id());

	Modules modules(app_name, level, engine_asset_path);

#ifdef NE_PLATFORM_MACOSX
    const char* game_dll_path = "bin/" NE_BUILD_DIR "/CFD/libCFD.dylib";
    const char* editor_dll_path = "bin/" NE_BUILD_DIR "/TheUnpluggingRunner/libNextEngineEditor.dylib";
#elif NE_PLATFORM_WINDOWS
	const char* game_dll_path = "bin/" NE_BUILD_DIR "/CFD/CFD.dll";
	const char* editor_dll_path = "bin/" NE_BUILD_DIR "/TheUnpluggingRunner/NextEngineEditor.dll";
#endif
    //convert_thread_to_fiber();
    
    {
        Context& context = get_context();
        context.temporary_allocator = &get_thread_local_temporary_allocator();
        context.allocator = &default_allocator;
    }
    
#ifdef BUILD_STANDALONE
	Application game(modules, game_dll_path);
	game.init();
	game.run();
#else
	Application editor(modules, editor_dll_path);
	editor.init((void*)game_dll_path);

	editor.run();

#endif
    
    //convert_fiber_to_thread();
}

int main() {
	uint num_workers = 4;
	make_job_system(20, num_workers);

	JobDesc init_jobs[MAX_THREADS];
	uint init_jobs_on[MAX_THREADS];

	for (uint i = 0; i < num_workers - 1; i++) {
		init_jobs[i] = { init_workers, nullptr };
		init_jobs_on[i] = i + 1;
	}

	atomic_counter counter = 0;
	schedule_jobs_on({ init_jobs_on, num_workers-1 }, { init_jobs, num_workers - 1 }, &counter);
    
    convert_thread_to_fiber();
    wait_for_counter(&counter, 0);
    
    fiber_main(nullptr);

	destroy_job_system();

	return 0;
}
