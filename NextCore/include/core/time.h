#pragma once

#include "core/core.h"
#include <chrono>

struct Time {
	std::chrono::steady_clock::time_point last_frame;
	float delta_time;

	void CORE_API tick();
	CORE_API static double now();
};