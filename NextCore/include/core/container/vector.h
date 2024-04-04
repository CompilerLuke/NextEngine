#pragma once

#include "core/memory/allocator.h"
#include "core/container/slice.h"
#include <initializer_list>
#include <new>
#include <stdio.h>
#include <string.h>
#include <utility>
#include <stdlib.h>

#define BOUNDS_CHECKING

template<typename T>
struct vector {
	Allocator* allocator = nullptr;
	uint length = 0;
	uint capacity = 0;
	T* data = NULL;

	inline void reserve(uint count) {
		if (count > capacity) {
			if (!allocator) allocator = &get_allocator();
			T* data = (T*)allocator->allocate(sizeof(T) * count);

			if (this->data) {
                if constexpr (std::is_pod_v<T>) memcpy(data, this->data, sizeof(T) * this->length);
                else for(int i = 0; i < length; i++) new (&data[i]) T(std::move(this->data[i]));
            }
			allocator->deallocate(this->data);

			this->data = data;
			this->capacity = count;
		}
	}

	inline void resize(uint count) {
		if (count > capacity) {
			if (capacity == 0) reserve(count);
			else if (capacity * 2 < count) reserve(count);
			else reserve(capacity * 2);
		}

		int diff = count - length;
		for (int i = 0; i < diff; i++) {
			new (data + length + i) T();
		}
		//todo free elements

		length = count;
	}

	inline void append(T&& element) {
		if (length >= capacity) {
			if (capacity == 0) reserve(2);
			else reserve(capacity * 2);
		}
		
		T* ptr = new (&data[length++]) T(std::move(element));
	}

	inline void append(const T& element) {
		if (length >= capacity) {
			if (capacity == 0) reserve(2);
			else reserve(capacity * 2);
		}

		new (data + length++) T(element);
	}

	inline void operator+=(slice<T> concat) {
		uint new_length = length + concat.length;
		if (new_length >= capacity) {
			reserve(max(new_length, capacity*2));
		}

		for (uint i = 0; i < concat.length; i++) {
			new (data + length + i) T(concat[i]);
		}

		length = new_length;
	}

	inline T& last(int index = 1) {
#ifdef BOUNDS_CHECKING
		assert(length - index >= 0);
#endif 
		return this->data[length - index];
	}

	inline vector(std::initializer_list<T> l) {
		reserve((unsigned int)l.size());
		for (const auto& i : l) {
			append(i);
		}
	}

	inline T pop() {
#ifdef BOUNDS_CHECKING
		if (this->length == 0) abort();
#endif
		return std::move(this->data[--this->length]);
	}

	inline void clear() {
		for (int i = 0; i < length; i++) {
			data[i].~T();
		}
		this->length = 0;
	}

	inline void shrink_to_fit() {
		T* new_data = (T*)allocator->allocate(sizeof(T) * length);
		if (data) memcpy(new_data, data, sizeof(T) * length);
		allocator->deallocate(data);
		data = new_data;
		capacity = length;
	}

	inline vector(vector<T>&& other) {
		this->data = other.data;
		this->length = other.length;
		this->capacity = other.capacity;
		this->allocator = other.allocator;

		other.data = 0;
		other.length = 0;
		other.capacity = 0;

	}

	inline vector(const vector<T>& other) {
		this->reserve(other.length);
		this->allocator = other.allocator;
		this->length = other.length;

		for (unsigned int i = 0; i < other.length; i++) {
			new (this->data + i)T(other.data[i]);
		}
	}

	inline void shift(unsigned int amount) {
#ifdef BOUNDS_CHECKING
		if (this->length < amount) abort();
#endif

		this->length -= amount;

		for (int i = 0; i < length; i++) {
			memcpy(this->data + i, this->data + i + amount, sizeof(T));
		}
	}

	inline vector<T>& operator=(vector<T>&& other) {
		this->~vector();

		this->length = other.length;
		this->allocator = other.allocator;
		this->capacity = other.capacity;
		this->data = other.data;

		other.length = 0;
		other.data = NULL;
		other.capacity = 0;

		return *this;
	}
	
	inline void free_data() {
		for (unsigned int i = 0; i < length; i++) {
			data[i].~T();
		}
	}

	vector<T>& operator=(const slice<T> other) {
		free_data();

		this->reserve(other.length);
		this->length = other.length;

		for (int i = 0; i < other.length; i++) {
			new (this->data + i) T(other.data[i]);
		}

		return *this;
	}

	vector<T>& operator=(const vector<T>& other) {
		return (*this = (const slice<T>)other);
	}

	inline bool contains(const T& element) {
		for (unsigned int i = 0; i < length; i++) {
			if (this->data[i] == element) return true;
		}

		return false;
	}

	inline vector() {
	}

	inline vector(uint capacity) {
		reserve(capacity);
	}

	inline vector(uint count, T elem) {
		reserve(count);
		length = count;

		for (int i = 0; i < count; i++) {
			new (data + i) T(elem);
		}
	}

	inline ~vector() {
		free_data();
		if (allocator) allocator->deallocate(data);
	}

	ARRAY_INDEXING

	operator slice<T>() & {
		return { data, length };
	}

	operator const slice<T>() const {
		return { data, length };
	}
};
