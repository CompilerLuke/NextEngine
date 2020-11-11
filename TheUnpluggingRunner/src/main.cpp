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

void fiber_main(void* data) {
	LinearAllocator& permanent_allocator = get_thread_local_permanent_allocator(); 
	LinearAllocator& temporary_allocator = get_thread_local_temporary_allocator();

	permanent_allocator = LinearAllocator(mb(500));
	temporary_allocator = LinearAllocator(mb(500));

	Context context = {};
	context.temporary_allocator = &get_thread_local_temporary_allocator();
	context.allocator = &default_allocator;

	ScopedContext scoped_context(context);

	const char* level = "/Users/antonellacalvia/Desktop/Coding/NextEngine/TheUnpluggingGame/data/level1/";
    const char* engine_asset_path = "/Users/antonellacalvia/Desktop/Coding/NextEngine/NextEngine/data/";
	const char* app_name = "The Unplugging";

	//assert(get_worker_id() == 0);
	//printf("Linear allocator %p on main fiber\n", &get_temporary_allocator());

	Modules modules(app_name, level, engine_asset_path);
    modules.window
    
#ifdef NE_RELEASE
    const char* game_dll_path = "/Users/antonellacalvia/Desktop/Coding/NextEngine/bin/Release-macosx-x86_64/TheUnpluggingRunner/libTheUnpluggingGame.dylib";
    const char* editor_dll_path = "/Users/antonellacalvia/Desktop/Coding/NextEngine/bin/Release-macosx-x86_64/TheUnpluggingRunner/libNextEngineEditor.dylib";
#else
    const char* game_dll_path = "/Users/antonellacalvia/Desktop/Coding/NextEngine/bin/Debug-macosx-x86_64/TheUnpluggingRunner/libTheUnpluggingGame.dylib";
    const char* editor_dll_path = "/Users/antonellacalvia/Desktop/Coding/NextEngine/bin/Debug-macosx-x86_64/TheUnpluggingRunner/libNextEngineEditor.dylib";
#endif

    /*
#ifdef _DEBUG
	const char* game_dll_path = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Debug\\TheUnpluggingGame.dll";
	const char* engine_dll_path = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Debug\\NextEngineEditor.dll";
#else
	const char* game_dll_path = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Release\\TheUnpluggingGame.dll";
	const char* engine_dll_path = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Release\\NextEngineEditor.dll";
#endif*/

#ifdef BUILD_STANDALONE
	Application game(game_dll_path);
	game.init();
	game.run();
#else
	Application editor(modules, editor_dll_path);
	editor.init((void*)game_dll_path);

	editor.run();

#endif
}

void init_workers(void*) {
	//printf("Initialized worker %i\n", get_worker_id());
	get_thread_local_permanent_allocator() = LinearAllocator(mb(10));
	get_thread_local_temporary_allocator() = LinearAllocator(mb(100));

	//printf("Linear allocator %p on fiber %i\n", &get_thread_local_temporary_allocator(), get_worker_id());
}

int main() {
	uint num_workers = 1;
	make_job_system(20, num_workers);

	JobDesc init_jobs[MAX_THREADS];
	uint init_jobs_on[MAX_THREADS];

	for (uint i = 0; i < num_workers - 1; i++) {
		init_jobs[i] = { init_workers, nullptr };
		init_jobs_on[i] = i + 1;
	}

    convert_thread_to_fiber();
	atomic_counter counter = 0;
	schedule_jobs_on({ init_jobs_on, num_workers }, { init_jobs, num_workers - 1 }, &counter);
	
    convert_thread_to_fiber();
    wait_for_counter(&counter, 0);
    fiber_main(nullptr);
    convert_fiber_to_thread();

	destroy_job_system();

	return 0;
}
