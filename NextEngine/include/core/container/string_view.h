#pragma once

#include <string.h>
#include <assert.h>
#include "core/core.h"

inline char to_lower_case(char a) {
	if ((a >= 65) && (a <= 90))
		a = a + 32;
	return a;
}

struct string_view {
	const char* data = "";
	unsigned int length = 0;

	inline string_view() {}
	inline string_view(const char* data, unsigned int length) : data(data), length(length) {}

	inline string_view(const char* data) {
		this->data = data;
		this->length = strlen(data);
	}

	inline bool starts_with(string_view pre) {
		return length < pre.length ? false : strncmp(pre.data, data, pre.length) == 0;
	}

	inline bool starts_with_ignore_case(string_view pre) {
		if (length < pre.length) return false;

		for (int i = 0; i < pre.length; i++) {
			if (to_lower_case(pre.data[i]) != to_lower_case(data[i])) return false;
		}

		return true;
	}

	inline bool ends_with(string_view pre) {
		return length  < pre.length ? false : strncmp(pre.data, data + (length - pre.length), pre.length) == 0;
	}

	inline int find_last_of(char c) {
		for (int i = this->length - 1; i >= 0; i--) {
			if (this->data[i] == c) return i;
		}
		return -1;
	}

	inline int find(char c) const {
		for (int i = 0; i < this->length; i++) {
			if (this->data[i] == c) return i;
		}
		return -1;
	}

	inline unsigned int size() const { return this->length; }

	inline string_view sub(unsigned int start, unsigned int end) const {
		assert(end <= length);
		assert(start <= length);
		assert(start <= end);
		
		return { this->data + start, end - start };
	}

	inline const char* c_str() const {
		return this->data;
	}

	inline bool operator==(string_view other) const {
		return length == other.length && strncmp(data, other.data, length) == 0;
	}

	inline bool operator!=(string_view other) const {
		return !(*this == other);
	}

	inline char operator[](unsigned int i) {
		assert(i < length);
		return this->data[i];
	}
};

bool ENGINE_API string_to_uint(string_view str, unsigned int* number);
bool ENGINE_API string_to_int(string_view str, int* number);