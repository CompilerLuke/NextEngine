#pragma once

#include "core/core.h"
#include <math.h>
#include <glm/vec2.hpp>

using real = double;

struct vec2 {
	real x;
	real y;

	vec2() : x(0.0f), y(0.0f) {}
	vec2(real x, real y) : x(x), y(y) {}
	vec2(real x) : x(x), y(x) {}

	vec2 operator+(vec2 other) {
		return { x + other.x, y + other.y };
	}

	vec2 operator-(vec2 other) {
		return { x - other.x, y - other.y };
	}

	vec2 operator*(vec2 other) {
		return { x * other.x, y * other.y };
	}
	
	vec2 operator/(vec2 other) {
		return { x * other.x, y * other.y };
	}

	void operator+=(vec2 other) {
		*this = *this + other;
	}

	void operator-=(vec2 other) {
		*this = *this - other;
	}

	void operator*=(vec2 other) {
		*this = *this * other;
	}

	void operator/=(vec2 other) {
		*this = *this * other;
	}

	operator glm::vec2() { return glm::vec2(x, y);  }
};

inline real length(vec2 v) {
	return sqrtf(v.x*v.x + v.y*v.y);
}

inline real dot(vec2 a, vec2 b) {
	return a.x * b.x + a.y * b.y;
}

inline vec2 normalize(vec2 v) {
	return v / length(v);
}
