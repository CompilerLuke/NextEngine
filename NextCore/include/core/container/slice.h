#pragma once

#include "core/core.h"
#include <assert.h>

#define ARRAY_INDEXING \
	T& operator[](uint i) { \
		assert(i < length); \
		return data[i]; \
	} \
\
	const T& operator[](uint i) const { \
		assert(i < length); \
		return data[i]; \
	} \
\
	T* begin() { return data; } \
	T* end() { return data + length; } \
	const T* begin() const { return data; } \
	const T* end() const { return data + length; } \

template<typename T>
struct slice {
	T* data;	
	uint length;

	slice() : data(nullptr), length(0) {}
	slice(T& value) :  data(&value), length(1) {}
	slice(T* data, uint length) : data(data), length(length) {}

	ARRAY_INDEXING
};
