#include "stdafx.h"
#include "core/time.h"

using namespace std::chrono;

std::chrono::steady_clock::time_point get_time() {
	return std::chrono::high_resolution_clock::now();
}

double Time::now() {
	static std::chrono::steady_clock::time_point start = get_time();
	//This is dangerous, could give different time depending on the application, since it is statically linked!!
	
	duration<double, std::milli> elapsed = get_time() - start;
	return elapsed.count() / 1000;
}

void Time::tick() {
	auto now = get_time();

	std::chrono::duration<double, std::milli> delta = now - last_frame;

	delta_time = delta.count() / 1000.0; 
	last_frame = now;
}