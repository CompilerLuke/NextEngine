#include "stdafx.h"
#include "core/profiler.h"
#include "core/io/logger.h"

//TODO rewrite using custom time API

//Profiler
vector<Frame> Profiler::frames;
int Profiler::profile_depth;
bool Profiler::paused = false;

Frame& get_current_frame() {
	return Profiler::frames.last();
}

void Profiler::begin_profile() {
	if (paused) return;
	profile_depth++;
}

void Profiler::record_profile(const Profile& profile) {
	if (paused) return;

	if (profile_depth == 0) throw "Bad record!";
	profile_depth--;

	Frame& frame = get_current_frame();

	ProfileData data;
	data.name = profile.name;
	data.duration = profile.end_time - profile.start_time;
	data.profile_depth = profile_depth;
	data.start = profile.start_time - frame.start_of_frame;

	//log(profile.name, " took ", (float)data.duration.count(), " ms\n");

	frame.profiles.append(data);
}

void Profiler::begin_frame() {
	if (paused) return;
	auto current_time = std::chrono::high_resolution_clock::now();

	if (frames.length > 0) {
		Frame& frame = get_current_frame();
		std::chrono::duration<double, std::milli> duration = current_time - frame.start_of_frame;

		frame.frame_swap_duration = duration.count();
	}

	Frame frame;
	frame.start_of_frame = current_time;

	if (frames.length >= 60)
		frames.shift(1);


	frames.append(std::move(frame));

	profile_depth = 0;
}

void Profiler::end_frame() {
	if (paused) return;
	Frame& current_frame = get_current_frame();

	std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - current_frame.start_of_frame;

	current_frame.frame_duration = duration.count();
}

//Profile
Profile::Profile(const char* name) {
	this->name = name;
	this->start_time = std::chrono::high_resolution_clock::now();
	this->ended = false;

	Profiler::begin_profile();
};

void Profile::end() {
	this->ended = true;
	this->end_time = std::chrono::high_resolution_clock::now();

	Profiler::record_profile(*this);
}

Profile::~Profile() {
	if (!ended) end();
};

void record_profile(Profile&);