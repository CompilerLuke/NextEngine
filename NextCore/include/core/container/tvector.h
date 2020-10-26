#pragma once

#include "core/memory/linear_allocator.h"
#include "core/container/slice.h"
#include <string.h>
#include <new>

// REMINDER - NO RAII and no Polymorphic Allocator - HIGH PERFOMANCE
template<typename T>
struct tvector {
	T* data = NULL;
	uint length = 0;
	uint capacity = 0;
	LinearAllocator* allocator = NULL;

	inline void reserve(unsigned int count) {
		if (count > capacity) {
			if (!allocator) allocator = &get_temporary_allocator();
			T* data = (T*)allocator->allocate(sizeof(T) * count);

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

	inline void operator+=(slice<T> other) {
		uint new_length = length + other.length;
		reserve(new_length);

		memcpy(data + length, other.data, sizeof(T) * other.length);
		length = new_length;
	}

	inline void clear() {
		length = 0;
		capacity = 0;
		data = 0;
	}

	T pop() { return data[--length]; }

	inline T& last() { return data[length - 1]; }

	ARRAY_INDEXING

	operator slice<T>() { return { data, length }; }
	operator const slice<T>() const { return { data, length }; }
};