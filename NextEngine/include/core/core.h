#pragma once

#ifdef NEXTENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#endif
#ifndef NEXTENGINE_EXPORTS
#define ENGINE_API __declspec(dllimport)
#endif

using uint = unsigned int;
using u64 = unsigned long long;
using i64 = long long;
using f64 = double; 

#define kb(num) num * 1024
#define mb(num) kb(num) * 1024
#define gb(num) mb(num) * 1024l