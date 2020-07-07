#pragma once

#include "core/core.h"
#include <GLFW/glfw3.h>
#include <chrono>

double get_time_seconds();

struct Time {
	std::chrono::steady_clock::time_point last_frame;
	float delta_time;

	void CORE_API tick();
};