#pragma once

#include <string.h>
#include <assert.h>

struct StringView {
	const char* data = "";
	unsigned int length = 0;

	inline StringView() {}
	inline StringView(const char* data, unsigned int length) : data(data), length(length) {}

	inline StringView(const char* data) {
		this->data = data;
		this->length = strlen(data);
	}

	StringView(const struct StringBuffer&);

	inline bool starts_with(StringView pre) {
		return length < pre.length ? false : strncmp(pre.data, data, pre.length) == 0;
	}

	inline bool ends_with(StringView pre) {
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

	inline StringView sub(unsigned int start, unsigned int end) const {
		assert(end <= length);
		assert(start <= length);
		assert(start <= end);
		
		return { this->data + start, end - start };
	}

	inline const char* c_str() const {
		return this->data;
	}

	inline bool operator==(StringView other) const {
		return length == other.length && strncmp(data, other.data, length) == 0;
	}

	inline bool operator!=(StringView other) const {
		return !(*this == other);
	}

	inline char operator[](unsigned int i) {
		assert(i < length);
		return this->data[i];
	}
};

bool string_to_uint(StringView str, unsigned int* number);
bool string_to_int(StringView str, int* number);