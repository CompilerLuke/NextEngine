#pragma once

#include "vec3.h"

struct vec4 {
	float x;
	float y;
	float z;
	float w;

	vec4() : x(0), y(0), z(0) {}
	vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	vec4(vec3 xyz, float w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}

	inline operator vec3() { return vec3(x, y, z); }
	inline float& operator[](uint i) { return (&this->x)[i]; }
};