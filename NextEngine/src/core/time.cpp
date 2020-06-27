#include "core/time.h"
#include "ecs/system.h"

std::chrono::steady_clock::time_point get_time() {
	return std::chrono::high_resolution_clock::now();
}

void Time::tick() {
	auto now = get_time();

	std::chrono::duration<double, std::milli> delta = now - last_frame;

	delta_time = delta.count() / 1000.0; 
	last_frame = now;
}