#include <chrono>

#ifdef PROFILING
#define PROFILE_BEGIN() { auto start = std::chrono::high_resolution_clock::now();

#define PROFILE_END(name) auto end = std::chrono::high_resolution_clock::now(); \
std::chrono::duration<double, std::milli> delta = end - start; \
float name = delta.count();

#define PROFILE_LOG(name) log(#name, " ", name, "ms"); }

#endif 
#ifndef PROFILING
#define PROFILE_BEGIN()
#define PROFILE_END(name)
#define PROFILE_LOG(name)
#endif