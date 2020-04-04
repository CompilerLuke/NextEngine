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

#define kb(num) num * 1024
#define mb(num) kb(num) * 1024
#define gb(num) mb(num) * 1024l