#pragma once

#include "core/core.h"
#include "core/container/vector.h"
#include "core/container/string_view.h"

struct CORE_API Profile {
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

struct Profiler {
	static bool CORE_API paused;
	static vector<Frame> CORE_API frames;
	static int CORE_API profile_depth;
	static uint CORE_API frame_sample_count;

	static void CORE_API begin_frame();
	static void CORE_API end_frame();

	static void CORE_API set_frame_sample_count(uint);
	static void CORE_API begin_profile();
	static void CORE_API record_profile(const Profile&);
};