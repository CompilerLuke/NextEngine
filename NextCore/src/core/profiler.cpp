#include "stdafx.h"
#include "core/profiler.h"
#include "core/io/logger.h"
#include "core/time.h"
#include "core/job_system/job.h"

//Profiler
vector<Frame> Profiler::frames[MAX_THREADS];
int Profiler::profile_depth[MAX_THREADS];
bool Profiler::paused = false;
uint Profiler::frame_sample_count = 500;

Frame& get_current_frame() {
	return Profiler::frames[get_worker_id()].last();
}

void Profiler::begin_profile() {
	if (paused) return;
	profile_depth[get_worker_id()]++;
}

void Profiler::record_profile(const Profile& profile) {
	uint worker_id = get_worker_id();
	if (worker_id != 0) return; //temporary
	
	if (paused) return;

	if (profile_depth[worker_id] == 0) throw "Bad record!";
	profile_depth[worker_id]--;

	Frame& frame = get_current_frame();

	ProfileData data;
	data.name = profile.name;
	data.duration = profile.end_time - profile.start_time;
	data.profile_depth = profile_depth[worker_id];
	data.start = profile.start_time - frame.start_of_frame;

	//log(profile.name, " took ", (float)data.duration.count(), " ms\n");

	frame.profiles.append(data);
}

void set_sample_count(uint& count) {
	uint worker = get_worker_id(); //dangerous

	if (count < Profiler::frames[worker].length) Profiler::frames[worker].shift(Profiler::frames[worker].length - count);
}

/*
uint worker_threads = worker_thread_count();
	uint worker = get_worker_id();

	JobDesc desc[MAX_THREADS];
	uint on_worker[MAX_THREADS];
	for (uint i = 0; i < worker_threads; i++) {
		if (i == worker) continue;
		desc[i] = JobDesc(set_sample_count, &count);
		on_worker[i] = i;
	}

	atomic_counter counter = 0;
	schedule_jobs_on({ on_worker, worker_threads - 1 }, { desc, worker_threads - 1 }, nullptr);
	set_sample_count(count);

	wait_for_counter(&counter);

	frame_sample_count = count;
*/

void Profiler::set_frame_sample_count(uint count) {
	uint worker_threads = worker_thread_count();
	uint worker = get_worker_id();

	if (count < frames[worker].length) frames[worker].shift(frames[worker].length - count);
}

/*
void begin_profiler_frame() {
	uint worker = get_worker_id();

	if (frames[worker].length > 0) {
		Frame& frame = frames[worker].last();
		double duration = current_time - frame.start_of_frame;

		frame.frame_swap_duration = duration;
	}

	Frame frame;
	frame.start_of_frame = current_time;

	if (frames[worker].length >= frame_sample_count)
		frames[worker].shift(1);

	frames[worker].append(std::move(frame));

	profile_depth[worker] = 0;
}*/

void Profiler::begin_frame() {
	if (paused) return;
	auto current_time = Time::now();

	uint worker_threads = worker_thread_count();
	uint worker = get_worker_id();

	if (frames[worker].length > 0) {
		Frame& frame = frames[worker].last();
		double duration = current_time - frame.start_of_frame;

		frame.frame_swap_duration = duration;
	}

	Frame frame;
	frame.start_of_frame = current_time;

	if (frames[worker].length >= frame_sample_count)
		frames[worker].shift(1);

	frames[worker].append(std::move(frame));

	profile_depth[worker] = 0;
}

void Profiler::end_frame() {
	if (paused) return;
	Frame& current_frame = get_current_frame();

	double duration = Time::now() - current_frame.start_of_frame;
	current_frame.frame_duration = duration;
}

//Profile
Profile::Profile(const char* name) {
	this->name = name;
	this->start_time = Time::now();
	this->ended = false;

	Profiler::begin_profile();
};

void Profile::end() {
	this->ended = true;
	this->end_time = Time::now();

	Profiler::record_profile(*this);
}

Profile::~Profile() {
	if (!ended) end();
};

void record_profile(Profile&);
