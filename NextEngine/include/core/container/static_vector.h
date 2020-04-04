#pragma once

#include "core/core.h"

template<int N, typename T>
struct static_vector {
	T data[N];
	uint length = 0;

	static_vector() {

	}

	void resize(int length) {
		assert(length < N);
		this->length = length;
	}

	static_vector(int length) {
		resize(length);
	}

	/*static_vector(const static_vector<N, T>& other) {
	length = other.length;
	for (int i = 0; i < length; i++) {
	new (data + i) T(other[i]);
	}
	}*/

	static_vector(static_vector<N, T>&& other) {
		length = other.length;
		memcpy(data, other.data, sizeof(T) * length);

		other.length = 0;
	}

	/*static_vector<N, T>& operator=(const static_vector<N, T>& other) {
	length = other.length;
	for (int i = 0; i < length; i++) new (data + i) T(other[i]);
	return *this;
	}*/

	static_vector<N, T>& operator=(static_vector<N, T>&& other) {
		length = other.length;
		memcpy(data, other.data, sizeof(T) * length);

		other.length = 0;

		return *this;
	}

	T& operator[](uint index) { return data[index]; }
	const T& operator[](uint index) const { return data[index]; }

	~static_vector() {
		for (int i = 0; i < length; i++) {
			data[i].~T();
		}
	}

	void append(const T& elem) {
		new (data + length++) T(elem);
	}

	void append(T&& elem) {
		new (data + length++) T(std::move(elem));
	}

	T pop() { return std::move(data[--length]); }

	inline T* begin() { return this->data; }
	inline T* end() { return this->data + length; }

	inline const T* begin() const { return this->data; }
	inline const T* end() const { return this->data + length; }

	operator slice<T>() { return { data, length }; }
};