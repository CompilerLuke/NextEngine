#pragma once

#include "string_view.h"

struct sstring {
	static constexpr int N = 50;

	char data[N];
	
	inline void length(int l) {
		data[N - 1] = N - l;
	}

	inline sstring() {
		length(0);
	}

	inline sstring(const char* str) {
		int i = 0;
		while (*str) {
			assert(i < N);
			data[i++] = *str++;
		}

		length(i);
	}

	inline sstring(string_view str) {
		assert(str.length <= N);
		memcpy(data, str.data, str.length + 1);
		length(str.length);
	}

	inline uint length() const {
		return N - data[N - 1];
	}

	inline char operator[](int i) {
		assert(i < N);
		return data[i];
	}

	inline bool operator==(string_view other) const {
		uint len = length();
		return len == other.length && strncmp(data, other.data, len) == 0;
	}

	inline bool operator!=(string_view other) const {
		return !(*this == other);
	}

	operator string_view() const {
		return { data, length() };
	}
};

inline uint hash_func(sstring& sstring) {
	const char* str = sstring.data;
	unsigned long hash = 5381;
	uint c;

	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}