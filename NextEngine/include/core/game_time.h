#pragma once

#include "ecs/system.h"
#include <chrono>

struct ENGINE_API Time {
	std::chrono::steady_clock::time_point last_frame;
	
	void update_time(UpdateParams&);
};

