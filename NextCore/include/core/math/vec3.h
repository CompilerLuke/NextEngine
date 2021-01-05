#pragma once

#include "core/core.h"
#include <math.h>
#include <glm/vec3.hpp>

struct vec3 {
	float x;
	float y;
	float z;

	inline vec3() : x(0.0f), y(0.0f), z(0.0f) {}
	inline vec3(float x, float y, float z) : x(x), y(y), z(z) {}
	inline vec3(float x) : x(x), y(x), z(x) {}

	inline vec3(glm::vec3 v) : x(v.x), y(v.y), z(v.z) {}
	inline operator glm::vec3() { return glm::vec3(x,y,z); }
};

inline vec3 operator+(vec3 a, vec3 b) {
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline vec3 operator-(vec3 a, vec3 b) {
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline vec3 operator-(vec3 a) {
    return { -a.x, -a.y, -a.z };
}

inline vec3 operator*(vec3 a, vec3 b) {
	return { a.x * b.x, a.y * b.y, a.z * b.z };
}

inline vec3 operator/(vec3 a, vec3 b) {
	return { a.x / b.x, a.y / b.y, a.z / b.z };
}

inline bool operator==(vec3 a, vec3 b) {
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool operator>(vec3 a, vec3 b) {
	return a.x > b.x && a.y > b.y && a.z > b.z;
}

inline bool operator<(vec3 a, vec3 b) {
	return a.x < b.x&& a.y < b.y&& a.z < b.z;
}

inline void operator+=(vec3& a, vec3 b) {
	a = a + b;
}

inline void operator-=(vec3& a, vec3 b) {
	a = a - b;
}

inline void operator*=(vec3& a, vec3 b) {
	a = a * b;
}

inline void operator/=(vec3& a, vec3 b) {
	a = a / b;
}

inline float length(vec3 v) {
	return sqrtf(v.x * v.x + v.y * v.y + v.z*v.z);
}

inline float dot(vec3 a, vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline vec3 normalize(vec3 v) {
	return v / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline vec3 cross(vec3 a, vec3 b) {
	return {
		a.y*b.z - a.z*b.y,
		a.z*b.x - a.x*b.z,
		a.x*b.y - a.y*b.x
	};
}

