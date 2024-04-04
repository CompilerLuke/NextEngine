#pragma once

#include <chrono>
#include "core/core.h"

struct Time {
	std::chrono::high_resolution_clock::time_point last_frame;
	float delta_time;

	void CORE_API tick();
	CORE_API static double now();
};