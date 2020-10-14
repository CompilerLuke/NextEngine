#include "core/profiler.h"
#include "core/io/logger.h"
#include "core/time.h"

//Profiler
vector<Frame> Profiler::frames;
int Profiler::profile_depth;
bool Profiler::paused = false;
uint Profiler::frame_sample_count = 500;

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

void Profiler::set_frame_sample_count(uint count) {
	if (count < frames.length) {
		frames.shift(frames.length - count);
	}

	frame_sample_count = count;
}

void Profiler::begin_frame() {
	if (paused) return;
	auto current_time = Time::now();

	if (frames.length > 0) {
		Frame& frame = get_current_frame();
		double duration = current_time - frame.start_of_frame;

		frame.frame_swap_duration = duration;
	}

	Frame frame;
	frame.start_of_frame = current_time;

	if (frames.length >= frame_sample_count)
		frames.shift(1);


	frames.append(std::move(frame));

	profile_depth = 0;
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
