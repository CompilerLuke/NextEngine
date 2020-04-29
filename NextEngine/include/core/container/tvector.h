#pragma once

#include "core/memory/linear_allocator.h"
#include "core/container/slice.h"

// REMINDER - NO RAII and no Polymorphic Allocator - HIGH PERFOMANCE
template<typename T>
struct tvector {
	T* data = NULL;
	uint length = 0;
	uint capacity = 0;

	inline void reserve(unsigned int count) {
		if (count > capacity) {
			T* data = TEMPORARY_ARRAY(T, count);

			if (this->data) memcpy(data, this->data, sizeof(T) * this->length);

			this->data = data;
			this->capacity = count;
		}
	}

	inline void append(const T& element) {
		if (length >= capacity) {
			if (capacity == 0) reserve(2);
			else reserve(capacity * 2);
		}
		new (&data[length++]) T(element);
	}

	const T& operator[](uint index) const { return data[index]; }
	T& operator[](uint index) { return data[index]; }
	T pop() { return data[--length]; }

	inline const T* begin() const { return this->data; }
	inline const T* end() const { return this->data + length; }
	inline T* begin() { return this->data; }
	inline T* end() { return this->data + length; }

	operator slice<T>() { return { data, length }; }
};