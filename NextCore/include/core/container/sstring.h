#pragma once

#include "string_view.h"

struct sstring {
	static constexpr int N = 48;

	char data[N];
	
	inline void length(int l) {
		data[N - 1] = N - l;
	}

	inline sstring() {
		length(0);
	}
    
    const char* c_str() const {
        return data;
    }

	inline sstring(const char* str) {
		int i = 0;
		while (*str) {
			assert(i < N);
			data[i++] = *str++;
		}
		data[i] = '\0';

		length(i);
	}

	inline sstring(string_view str) {
		assert(str.length <= N);
		memcpy(data, str.data, (size_t)str.length + 1);
		length(str.length);
	}

	inline uint length() const {
		return N - data[N - 1];
	}

	inline char operator[](int i) {
		assert(i < N);
		return data[i];
	}

	inline void operator+=(string_view other) {
		uint curr_len = length();
		uint new_len = curr_len + other.length;
		assert(new_len < N);

		memcpy(data + curr_len, other.data, other.length);
		data[new_len] = '\0';

		length(new_len);
	}

	inline bool operator==(string_view other) const {
		uint len = length();
		return len == other.length && strncmp(data, other.data, len) == 0;
	}

	inline bool operator!=(string_view other) const {
		return !(*this == other);
	}

	operator string_view() const& {
		return { data, length() };
	}
};
