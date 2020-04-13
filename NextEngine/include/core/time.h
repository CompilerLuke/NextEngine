#pragma once

#include "ecs/system.h"
#include <GLFW/glfw3.h>
#include <chrono>

struct Time {
	std::chrono::steady_clock::time_point last_frame;
	float delta_time;

	void ENGINE_API tick();
};