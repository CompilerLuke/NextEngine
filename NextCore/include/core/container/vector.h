#pragma once

#include "core/memory/allocator.h"
#include "core/container/slice.h"
#include <initializer_list>
#include <new>
#include <stdio.h>
#include <string.h>


#define BOUNDS_CHECKING

template<typename T>
struct vector {
	Allocator* allocator = &default_allocator;
	uint length = 0;
	uint capacity = 0;
	T* data = NULL;

	inline void reserve(uint count) {
		if (count > capacity) {
			T* data = (T*)allocator->allocate(sizeof(T) * count);

			if (this->data) memcpy(data, this->data, sizeof(T) * this->length);

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
		
		T* ptr = new (&data[length++]) T();
		*ptr = std::move(element);
	}

	inline void append(const T& element) {
		if (length >= capacity) {
			if (capacity == 0) reserve(2);
			else reserve(capacity * 2);
		}

		new (data + length++) T(element);
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

	inline T& operator[](unsigned int index) {

#ifdef BOUNDS_CHECKING
		if (index >= length) abort();
#endif

		return data[index];
	}

	inline const T& operator[](unsigned int index) const {
#ifdef BOUNDS_CHECKING
		if (index >= length) abort();
#endif

		return data[index];
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


	vector<T>& operator=(const vector<T>& other) {
		free_data();

		this->reserve(other.length);
		this->length = other.length;

		for (int i = 0; i < other.length; i++) {
			new (this->data + i) T(other.data[i]);
		}

		return *this;
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
		allocator->deallocate(data);
	}

	inline T* begin() {
		return this->data;
	}

	inline T* end() {
		return this->data + length;
	}

	inline const T* begin() const {
		return this->data;
	}

	inline const T* end() const {
		return this->data + length;
	}

	operator slice<T>() {
		return { data, length };
	}
};