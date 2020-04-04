#pragma once

#include "core/core.h"
#include <assert.h>

template<typename T>
struct slice {
	T* data;	
	uint length;

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
