#include "stdafx.h"
#include "core/vector.h"

template<typename T, unsigned int N>
struct static_vector {
	unsigned int length = 0;
	T data[N];

	inline void append(const T& element) {
		data[length++] = element;
	}

	inline void append(T&& element) {
#ifdef BOUNDS_CHECKING
		if (length + 1 >= N) abort();
#endif 

		new (&data[length++]) T(std::move(element));
	}

	inline T pop() {
#ifdef BOUNDS_CHECKING
		if (this->length == 0) abort();
#endif
		return std::move(this->data[this->length - 1]);
	}

	inline void clear() {
		for (int i = 0; i < length; i++) {
			data[i].~T();
		}
		this->length = 0;
	}

	T& operator[](int index) {

#ifdef BOUNDS_CHECKING
		if (index < 0 && index >= length) abort();
#endif

		return data[index];
	}

	const T& operator[](int index) const {
#ifdef BOUNDS_CHECKING
		if (index < 0 && index >= length) abort();
#endif

		return data[index];
	}

	inline static_vector(static_vector<T, N>&& other) {
		this->data = other.data;
		this->length = other.length;

		other.length = 0;
	}

	inline static_vector(const static_vector<T, N>& other) {
#ifdef BOUNDS_CHECKING
		if (other->length >= N) abort();
#endif 
		this->length = other.length;
		memcpy(this->data, other.data, sizeof(T) * length);
	}

	inline void shift(unsigned int amount) {
#ifdef BOUNDS_CHECKING
		if (this->length < amount) abort();
#endif

		this->length -= amount;
		memcpy(this->data, &this->data[amount], sizeof(T) * this->length);
	}

	static_vector<T, N>& operator=(const static_vector<T, N>& other) {
		this->length = other.length;

		memcpy(this->data, other.data, sizeof(T) * length);

		return *this;
	}

	inline static_vector() {
	}

	inline ~static_vector() {
		for (int i = 0; i < length; i++) {
			data[i].~T();
		}
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