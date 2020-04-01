#pragma once

#include "core/core.h"

template<typename T>
struct slice {
	uint length;
	T* data;

	T& operator[](int i) {
		assert(i < length);
		return data[i];
	}

	const T& operator[](int i) const {
		assert(i < length);
		return data[i];
	}

	T* begin() { return data; }
	T* end() { return data + length; }
	const T* begin() const { return data; }
	const T* end() const { return data + length; }
};
