#pragma once

#include "core/core.h"
#include "core/container/slice.h"

//NOTE THIS LIMITS THE MAXIMAL OFFSET TO BE 4GB
template<typename T>
struct offset_slice {
	uint offset;
	uint length;

	offset_slice() : offset(0), length(0) {}
	offset_slice(T* data, uint length) : offset((u8*)this - (u8*)data), length(length) {}

	T* data() { return (T*)((u8*)this + offset); }
	const T* data() const { return (T*)((u8*)this + offset); }

	T& operator[](uint i) { return data()[i]; }
	const T& operator[](uint i) const { return data()[i]; }

	T* begin() { return data(); }
	T* end() { return data() + length; }

	const T* begin() { return data(); }
	const T* end() { return data() + length; }

	operator slice<T>() { return { data(), length }; }
};