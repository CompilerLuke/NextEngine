#pragma once

#include "core/container/vector.h"
#include "core/container/string_view.h"

struct ENGINE_API Profile {
	double start_time;
	double end_time;

	const char* name;
	bool ended;

	Profile(const char* name);

	void end();

	~Profile();
};

struct ProfileData {
	const char* name;
	double start;
	double duration;
	int profile_depth;
};

struct Frame {
	double start_of_frame;
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