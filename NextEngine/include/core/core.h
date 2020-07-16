#pragma once

#include <stdint.h>

#ifdef NEXTENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#endif
#ifndef NEXTENGINE_EXPORTS
#define ENGINE_API __declspec(dllimport)
#endif

using uint = uint32_t;
using u64 = uint64_t;
using i64 = int64_t;
using f64 = double; 
using u8 = uint8_t;
using u16 = uint16_t;

#define kb(num) num * 1024
#define mb(num) kb(num) * 1024
#define gb(num) mb(num) * 1024l

#define COMP
#define REFL
#define REFL_FALSE

#undef max
#undef min

//PER FRAME RESOURCES HAVE TO BE DUPLICATED, OTHERWISE THE GPU CAN GRAB DATA FROM THE WRONG FRAME
const int MAX_FRAMES_IN_FLIGHT = 3;

inline constexpr uint max(uint a, uint b) {
	return a > b ? a : b;
}

inline constexpr uint min(uint a, uint b) {
	return a < b ? a : b;
}