#pragma once

#include "core/core.h"
#include <math.h>
#include <glm/vec3.hpp>

#undef min
#undef max

using real = double;

//todo merge vec3.h and vec4.h into one file

struct vec3 {
	real x;
	real y;
	real z;

	inline vec3() : x(0.0f), y(0.0f), z(0.0f) {}
	inline vec3(real x, real y, real z) : x(x), y(y), z(z) {}
	inline vec3(real x) : x(x), y(x), z(x) {}

	inline vec3(glm::vec3 v) : x(v.x), y(v.y), z(v.z) {}
	inline operator glm::vec3() const { return glm::vec3(x,y,z); }
    inline real& operator[](uint i) { return (&this->x)[i]; }
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

inline bool operator!=(vec3 a, vec3 b) {
	return a.x != b.x || a.y != b.y || a.z != b.z;
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

inline float sq(vec3 a) {
	return a.x*a.x + a.y*a.y + a.z*a.z;
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

inline vec3 abs(vec3 vec) {
    return vec3(fabs(vec.x), fabs(vec.y), fabs(vec.z));
}

inline float max(vec3 vec) {
    if (vec.x > vec.y) return vec.x > vec.z ? vec.x : vec.z;
    else return vec.y > vec.z ? vec.y : vec.z;
}

constexpr double PI = 3.1415926535897932384626433832795028;

//todo: move into seperate file
inline float to_radians(float angle) {
	return angle * PI / 180;
}

inline float to_degrees(float angle) {
	return angle * 180 / PI;
}

//todo move to seperate file
inline real clamp(real low, real high, real value) {
	if (value > high) return high;
	if (value < low) return low;
	return value;
}

inline real vec_angle_cos(vec3 v0, vec3 v1, vec3 v2) {
	vec3 v01 = v0 - v1;
	vec3 v21 = v2 - v1;
	return clamp(-1, 1, dot(v01, v21) / (length(v01) * length(v21)));
}

inline real vec_angle(vec3 v0, vec3 v1, vec3 v2) {
	return acosf(vec_angle_cos(v0, v1, v2));
}

inline real vec_sign_angle(vec3 v0, vec3 v1, vec3 vn) {
	real angle = acos(clamp(-1, 1, dot(v0, v1) / (length(v0) * length(v1))));

	vec3 v2 = cross(-v0, v1);
	if (dot(vn, v2) < -FLT_EPSILON) { // Or > 0
		angle = -angle;
	}
	return angle;
}

inline real vec_dir_angle(vec3 v0, vec3 v1, vec3 vn) {
	real angle = vec_sign_angle(v0, v1, vn);
	if (angle < 0) { 
		angle = 2 * PI + angle;
	}
	return angle;
}

inline real vec_angle(vec3 v0, vec3 v1) {
	return acosf(clamp(-1, 1, dot(v0, v1) / (length(v0) * length(v1))));
}