#include "stdafx.h"
#include "core/job_system/fiber.h"
#include "core/job_system/job.h"
#include "core/job_system/work_stealing_queue.h"
#include "core/container/queue.h"
#include "core/container/array.h"


#include <thread>

thread_local worker_handle worker_handle;

constexpr uint MAX_FIBERS = 1000;
constexpr uint MAX_JOBS = 10000;

struct Job {
	JobFunc func;
	void* data;
	atomic_counter* counter;
};

struct ParkedFiber {
	Fiber* fiber;
	atomic_counter* counter;
	uint value;
};

using JobQueue = work_stealing_queue<MAX_JOBS, Job>; //todo change order
using FiberPool = array<MAX_FIBERS, Fiber*>;
using WaitList = array<MAX_FIBERS, ParkedFiber>;

//JOB SYSTEM DATA
//note: no job stealing, fibers always remain on the same thread
std::atomic<bool> workers_exit;
array<MAX_THREADS, std::thread> workers;
JobQueue queues[MAX_THREADS][PRIORITY_COUNT] = {};
queue<Job, 10> private_queue[MAX_THREADS] = {};

FiberPool fiber_pools[MAX_THREADS] = {};
WaitList wait_lists[MAX_THREADS] = {};



uint hardware_thread_count() {
	return std::thread::hardware_concurrency();
}

uint worker_thread_count() {
	return workers.length;
}

uint get_worker_id() {
	assert(worker_handle.id);
	return worker_handle.id - 1;
}

Fiber* alloc_fiber() {
	FiberPool& pool = fiber_pools[get_worker_id()];
	
	if (pool.length == 0) {
		printf("Run out of fibers!!\n");
		abort();
	}

	return pool.pop();
}

void dealloc_fiber(Fiber* fiber) {
	FiberPool& pool = fiber_pools[get_worker_id()];
	pool.append(fiber);
}


void execute(Job job) {
	if (job.func == nullptr) {
		printf("Executing null job!");
		abort();
	}

	//printf("dequeued job #%i\n", counter++);
    assert(job.func);
	job.func(job.data);
	if (job.counter) {
		uint current = --(*job.counter);
		/*if (current > 1000) {
			printf("Unsigned overflow!! %u\n", current);
			abort();
		}*/
	}

	//printf("Running job on %i, cpu %i\n", get_worker_id(), GetCurrentProcessorNumber());
}

bool pop_job(uint worker, Job* job) {
	assert(get_worker_id() == worker);

	if (private_queue[worker].dequeue(job)) return true; //todo what priority level should this be

	for (uint priority = 0; priority < PRIORITY_COUNT; priority++) {
		if (queues[worker][priority].pop(job)) return true;
	}

	return false;
}

bool steal_job(uint worker, Job* job) {
	for (uint priority = 0; priority < PRIORITY_COUNT; priority++) {
		if (queues[worker][priority].steal(job)) return true;
	}

	return false;
}

void run_fiber(void* fiber) {
	uint worker = get_worker_id();
	WaitList& wait_list = wait_lists[worker];
	Fiber* worker_fiber = get_current_fiber();

	uint workers_len = workers.length;

	while (!workers_exit) {
		Job job = {};

		if (pop_job(worker, &job)) { //pop doesn't work reliably!
			execute(job);
		}
		else {
			bool resumed = false;

			for (uint i = 0; i < wait_list.length; i++) {
				uint counter = wait_list[i].counter->load(std::memory_order_relaxed);
				if (counter == wait_list[i].value) {
					Fiber* fiber = wait_list[i].fiber;
					wait_list.data[i] = wait_list.data[--wait_list.length];
					resumed = true;
					dealloc_fiber(worker_fiber);
					switch_to_fiber(fiber);
					break;
				}
			}
			
			if (!resumed) {
				uint steal_from = rand() % workers_len;

				for (uint i = 0; i < workers_len; i++) {
					if (steal_from != worker && steal_job(steal_from, &job)) break;
					steal_from = (steal_from + 1) % workers_len;
				}

				if (job.func) execute(job);
				else thread_sleep(1);
			}
		}
	}
}

void wait_for_counter(atomic_counter* counter, uint value) {
    assert(counter);
    if (counter->load(std::memory_order_relaxed) == value) return;
	
	uint worker_id = get_worker_id();
	WaitList& wait_list = wait_lists[worker_id];
	FiberPool& fiber_pool = fiber_pools[worker_id];
	Fiber* fiber = get_current_fiber(); //corrupts the stack??
    
    //assert(worker_id == 0);

	//todo what memory order should this be
	Fiber* yield_to = nullptr;

	for (uint i = 0; i < wait_list.length; i++) {
		if (wait_list[i].counter->load(std::memory_order_relaxed) == wait_list[i].value) {
			yield_to = wait_list[i].fiber;
			wait_list[i] = { fiber, counter, value };
			break;
		}
	}
		
	if (!yield_to) {
		yield_to = fiber_pool.pop();
		wait_list.append({fiber, counter, value});
	}

	switch_to_fiber(yield_to);
}

WaitCondition::WaitCondition() {
	fiber = get_current_fiber();
	counter = 0;
	value = 0;
}

void wait_for_jobs_on_thread(Priority priority, slice<JobDesc> desc) {
	convert_thread_to_fiber();
	wait_for_jobs(priority, desc);
	convert_fiber_to_thread();
}

void wait_for_counter_on_thread(atomic_counter* counter, uint value) {
	convert_thread_to_fiber();
	wait_for_counter(counter, value);
	convert_fiber_to_thread();
}


void run_worker(uint worker) {
	//printf("Starting worker %i\n", worker);

	worker_handle = { worker + 1 };

	convert_thread_to_fiber();
	run_fiber(nullptr);
}


FLS* fls_context;

void make_job_system(uint num_fibers, uint num_workers) {
	assert(num_workers <= MAX_THREADS);
	assert(num_fibers <= MAX_FIBERS);
	assert(num_workers <= hardware_thread_count());

	const uint main_thread = 0;

	
	for (uint worker = 0; worker < num_workers; worker++) {
		for (uint i = 0; i < num_fibers; i++) {
			fiber_pools[worker].append(make_fiber(kb(128), run_fiber));
		}

		if (worker == main_thread) workers.append(std::thread());
		else {
			//std::thread thread(run_worker, worker);
			//SetThreadAffinityMask(thread.native_handle(), (1 << worker));

			workers.append(std::thread(run_worker, worker));
		}
	}

	for (uint worker = 1; worker < num_workers; worker++) {
		workers[worker].detach();
	}

	worker_handle = { main_thread + 1 };
    fls_context = make_FLS(nullptr);
}

void destroy_job_system() {
    free_FLS(fls_context);

	workers_exit = true;

	workers.clear();
}

void schedule_jobs_on(slice<uint> workers, slice<JobDesc> jobs, atomic_counter* counter) {
	if (counter) *counter += jobs.length;

	//assert(workers.length == jobs.length);

	for (uint i = 0; i < jobs.length; i++) {
		while (!private_queue[workers[i]].enqueue({
			jobs[i].func,
			jobs[i].data,
			counter
        })) {
            thread_sleep(0);
        }
	}
}

void add_jobs(Priority priority, slice<JobDesc> jobs, atomic_counter* counter) {
	if (counter) *counter += jobs.length;

	uint worker = get_worker_id();
	Fiber* fiber = get_current_fiber();
	
	for (JobDesc& desc : jobs) { 
		//Could we enqueue multiple jobs at a time?
		//and save on sychronization
		//in the end it may not matter since they have to be read individually anyways
		
		//printf("enqueued job #%i\n", counter++);

		assert(queues[worker][priority].push({
			desc.func,
			desc.data,
			counter
		}));
	}
}

void wait_for_jobs(Priority priority, slice<JobDesc> jobs) {
	if (jobs.length == 0) return;
	if (jobs.length == 1) {
		jobs[0].func(jobs[0].data);
		return;
	}

	atomic_counter counter = {};
	add_jobs(priority, jobs, &counter);
	wait_for_counter(&counter, 0);
}



//CONTEXT

#include "core/context.h"

static thread_local Context thread_local_context;

Context& get_context() {
	Context* ctx = (Context*)get_FLS(fls_context);
    if (ctx) return *ctx;
    //assert(thread_local_context != nullptr);
	return thread_local_context; //Fallback setting thread local if not in fiber
}

void set_context(Context* context) {
	if (!set_FLS(fls_context, context)) {
		thread_local_context = *context; //fallback setting thread local if not in fiber
	}
}

ScopedContext::ScopedContext(Context& current) {
	this->current = current;

    if (Context* ctx = (Context*)get_FLS(fls_context)) this->previus = ctx;
    else this->previus = &thread_local_context;

	set_context(&current); 
}

ScopedContext::~ScopedContext() {
	set_FLS(fls_context, previus);
}
