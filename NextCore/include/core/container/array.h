#pragma once

#include "core/core.h"
#include "slice.h"
#include <initializer_list>
#include <utility>

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

	void shift(uint by) {
		assert(by <= length);
		length -= by;
		for (uint i = 0; i < length; i++) {
			data[i] = data[i + by];
		}

		//todo call destructors
	}

	array(array<N, T>&& other) {
		length = other.length;
		for (uint i = 0; i < length; i++) new (data + i)  T(std::move(other.data[i]));
		//memcpy(data, other.data, sizeof(T) * length);
		other.length = 0;
	}

	array(const array<N,T>& other) {
		length = other.length;
		for (int i = 0; i < length; i++) {
			new (data + i) T(other.data[i]);
		}
	}

	inline array(std::initializer_list<T> list) {
		for (const auto& i : list) {
			append(i);
		}
	}

	inline array(slice<T> list) {
		assert(list.length < N);
		length = list.length;
		
		for (int i = 0; i < length; i++) {
			data[i] = list[i];
		}
	}

	inline array(T* new_data, int count) {
		length = count;
		for (int i = 0; i < count; i++) {
			data[i] = new_data[i];
		}
	}

	inline bool contains(const T& element) {
		for (unsigned int i = 0; i < length; i++) {
			if (this->data[i] == element) return true;
		}

		return false;
	}

	array<N, T>& operator=(slice<T>& other) {
		assert(other.length <= N);
		length = other.length;
		for (int i = 0; i < length; i++) new (data + i) T(other[i]);
		return *this;
	}

	array<N, T>& operator=(const array<N, T>& other) {
		length = other.length;
		for (int i = 0; i < length; i++) new (data + i) T(other[i]);
		return *this;
	}

	array<N, T>& operator=(array<N, T>&& other) {
		length = other.length;
		for (uint i = 0; i < length; i++) new (data + i) T(std::move(other.data[i]));
		//memcpy(data, other.data, sizeof(T) * length);

		other.length = 0;

		return *this;
	}

	T& operator[](uint index) { 
		assert(index < length);
		return data[index]; 
	}
	
	const T& operator[](uint index) const { 
		assert(index < length);
		return data[index]; 
	}

	/*
	~array() {
		for (int i = 0; i < length; i++) {
			data[i].~T();
		}
	}*/

	void append(const T& elem) {
		new (data + length++) T(elem);
	}

	void append(T&& elem) {
		assert(length < N);
		new (data + length++) T(static_cast<T&&>(elem));
	}

	void clear() {
		for (int i = 0; i < length; i++) data[i].~T();		
		length = 0;
	}



	T pop() { assert(length > 0); return std::move(data[--length]); }

	inline T* begin() { return this->data; }
	inline T* end() { return this->data + length; }

	inline const T* begin() const { return this->data; }
	inline const T* end() const { return this->data + length; }

	inline T& last() { return data[length - 1]; }

    operator slice<T>() & { return { data, length }; }
    operator const slice<T>() const& { return { data, length }; }
};
