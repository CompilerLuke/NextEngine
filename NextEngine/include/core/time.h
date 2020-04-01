#pragma once

#include "ecs/system.h"
#include <GLFW/glfw3.h>
#include <chrono>

struct ENGINE_API Time {
	std::chrono::steady_clock::time_point last_frame;
	float delta_time;

	void tick();
};