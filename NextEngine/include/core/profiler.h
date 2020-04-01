#pragma once

#include "core/container/vector.h"
#include "core/container/string_view.h"
#include "core/time.h"

struct ENGINE_API Profile {
	std::chrono::high_resolution_clock::time_point start_time;
	std::chrono::high_resolution_clock::time_point end_time;

	const char* name;
	bool ended;

	Profile(const char* name);

	void end();

	~Profile();
};

struct ProfileData {
	const char* name;
	std::chrono::duration<double, std::milli> start;
	std::chrono::duration<double, std::milli> duration;
	int profile_depth;
};

struct Frame {
	std::chrono::high_resolution_clock::time_point start_of_frame;
	double frame_duration;
	double frame_swap_duration; //includes time waiting for vSync
	vector<ProfileData> profiles;
};

struct ENGINE_API Profiler {
	static bool paused;
	static vector<Frame> frames;
	static int profile_depth;

	static void begin_frame();
	static void end_frame();

	static void begin_profile();
	static void record_profile(const Profile&);
};