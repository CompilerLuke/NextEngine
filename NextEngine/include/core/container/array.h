#pragma once

#include "core/core.h"
#include "slice.h"
#include <initializer_list>

template<int N, typename T>
struct array {
	T data[N];
	uint length = 0;

	array() {

	}

	void resize(int length) {
		assert(length <= N);
		this->length = length;
	}

	array(int length) {
		resize(length);
	}

	array(array<N, T>&& other) {
		length = other.length;
		memcpy(data, other.data, sizeof(T) * length);

		other.length = 0;
	}

	array copy() {
		array result;
		result.length = length;
		for (int i = 0; i < length; i++) {
			new (result.data + i) T(data[i]);
		}
		return result;
	}


	inline array(std::initializer_list<T> list) {
		for (const auto& i : list) {
			append(i);
		}
	}

	inline array(T* new_data, int count) {
		length = count;
		for (int i = 0; i < count; i++) {
			data[i] = new_data[i];
		}
	}

	array<N, T>& operator=(const array<N, T>& other) {
		length = other.length;
		for (int i = 0; i < length; i++) new (data + i) T(other[i]);
		return *this;
	}

	array<N, T>& operator=(array<N, T>&& other) {
		length = other.length;
		memcpy(data, other.data, sizeof(T) * length);

		other.length = 0;

		return *this;
	}

	T& operator[](uint index) { return data[index]; }
	const T& operator[](uint index) const { return data[index]; }

	~array() {
		for (int i = 0; i < length; i++) {
			data[i].~T();
		}
	}

	void append(const T& elem) {
		new (data + length++) T(elem);
	}

	void append(T&& elem) {
		assert(length < N);
		new (data + length++) T(std::move(elem));
	}

	void clear() {
		for (int i = 0; i < length; i++) data[i].~T();		
		length = 0;
	}

	T pop() { return std::move(data[--length]); }

	inline T* begin() { return this->data; }
	inline T* end() { return this->data + length; }

	inline const T* begin() const { return this->data; }
	inline const T* end() const { return this->data + length; }

	operator slice<T>() { return { data, length }; }
};