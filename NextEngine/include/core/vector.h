#pragma once

#include "core/allocator.h"
#include <initializer_list>
#include <memory.h>
#include <type_traits>

#ifdef _DEBUG
#define BOUNDS_CHECKING
#endif 

#include <iterator>

template<typename T>
struct vec_iterator : std::iterator<std::random_access_iterator_tag, T> {
	const T* data = NULL;
	unsigned int index = 0;

	inline vec_iterator(const T* data, unsigned int index) : data(data), index(index) {

	}

	inline int operator-(const vec_iterator<T>& other) const {
		return this->index - other.index;
	}

	inline bool operator==(vec_iterator<T>& other) const {
		return other.index == index && other.data == data;
	}

	inline bool operator!=(vec_iterator<T>& other) const {
		return !(*this == other);
	}

	inline vec_iterator<T>& operator++() {
		index++;
		return *this;
	}

	inline vec_iterator<T>& operator--() {
		index--;
		return *this;
	}

	inline T& operator[](unsigned int i) const {
		return this->data[i];
	}

	inline T& operator*() {
		return (T&)(this->data[index]);
	}

	inline const T& operator*() const {
		return (T&)(this->data[index]);
	}
};

template<typename T>
struct vector {
	Allocator* allocator = &default_allocator;
	unsigned int length = 0;
	unsigned int capacity = 0;
	T* data = NULL;

	inline void reserve(unsigned int count) {
		if (count > capacity) {
			T* data = (T*)allocator->allocate(sizeof(T) * count);

			if (this->data) memcpy(data, this->data, sizeof(T) * this->length);

			allocator->deallocate(this->data);

			this->data = data;
			this->capacity = count;
		}
	}

	inline void append(T&& element) {
		if (length >= capacity) {
			if (capacity == 0) reserve(2);
			else reserve(capacity * 2);
		}
		new (&data[length++]) T(std::move(element));
	}

	inline void append(const T& element) {
		if (length >= capacity) {
			if (capacity == 0) reserve(2);
			else reserve(capacity * 2);
		}

		new (data + length++)T(element);
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
		if (index < 0 && index >= length) abort();
#endif

		return data[index];
	}

	inline const T& operator[](unsigned int index) const {
#ifdef BOUNDS_CHECKING
		if (index < 0 && index >= length) abort();
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

	inline vector<T>& operator=(const vector<T>& other) {
		free_data();

		this->allocator = other.allocator;
		this->reserve(other.length);
		this->length = other.length;

		for (int i = 0; i < other.length; i++) {
			new (this->data + i) T(other.data[i]);
		}

		return *this;
	}

	inline vector() {
	}

	inline ~vector() {
		free_data();
		allocator->deallocate(data);
	}

	inline vec_iterator<T> begin() {
		return vec_iterator<T>(this->data, 0);
	}

	inline vec_iterator<T> end() {
		return vec_iterator<T>(this->data, length);
	}

	inline vec_iterator<const T> begin() const {
		return vec_iterator<const T>(this->data, 0);
	}

	inline vec_iterator<const T> end() const {
		return vec_iterator<const T>(this->data, length);
	}
};