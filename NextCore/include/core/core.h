#pragma once

#include <stdint.h>
#include <float.h>
#include <stdio.h>
#include <math.h>

#if 0
#ifdef NE_PLATFORM_WINDOWS
	#ifdef NEXTCORE_EXPORTS
	#define CORE_API __declspec(dllexport)
	#else
	#define CORE_API __declspec(dllimport)
	#endif
#else 
	#define CORE_API
#endif
#else
	#define CORE_API
#endif

using uint = uint32_t;
using u64 = uint64_t;
using i64 = int64_t;
using f64 = double; 
using u8 = uint8_t;
using u16 = uint16_t;

#define kb(num) num * 1024
#define mb(num) kb(num) * 1024ull
#define gb(num) mb(num) * 1024ull

#define SYSTEM_COMP
#define COMP
#define REFL
#define REFL_FALSE

#define WARN(str, ...) fprintf(stderr, "WARNING - "#str, __VA_ARGS__)

#undef max
#undef min

//PER FRAME RESOURCES HAVE TO BE DUPLICATED, OTHERWISE THE GPU CAN GRAB DATA FROM THE WRONG FRAME
const uint MAX_FRAMES_IN_FLIGHT = 3;

inline constexpr int max(int a, int b) {
	return a > b ? a : b;
}

inline constexpr int min(int a, int b) {
	return a < b ? a : b;
}

inline constexpr int divceil(int a, int b) {
	return (a - 1) / b + 1;
}

template<typename T>
inline constexpr int signnum(T a) {
	return (a > 0) - (a < 0);
}

#define assert_mesg(cond, mesg) if (!(cond)) { fprintf(stderr, mesg); exit(1); }
