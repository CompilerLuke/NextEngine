#pragma once

#include "core/allocator.h"
#include <memory.h>
#include <string.h>
#include "core/vector.h"
#include "core/string_view.h"
#include "glm/glm.hpp"

struct StringBuffer {
	Allocator* allocator = &default_allocator;
	char* data = NULL;
	unsigned int capacity = 0;
	unsigned int length = 0;

	inline unsigned int size() {
		return length;
	}

	inline void reserve(unsigned int amount) {
		if (amount > this->capacity) {
			char* data = (char*)allocator->allocate(sizeof(char) * (amount + 1));
			
			if (this->data) memcpy(data, this->data, sizeof(char) * (this->length + 1));

			allocator->deallocate(this->data);
			this->data = data;
			this->capacity = amount;
		}
	}

	inline StringBuffer(const char* str) {
		unsigned int length = strlen(str);
		if (length == 0) return;
		this->reserve(length);
		this->length = length;
		memcpy(this->data, str, sizeof(char) * (length + 1));
	}

	inline StringBuffer(StringView str) {
		this->reserve(str.length);
		this->length = str.length;
		memcpy(this->data, str.data, length);
		data[length] = '\0';
	}

	inline StringBuffer(StringBuffer&& other) {
		this->data = other.data;
		this->length = other.length;
		this->capacity = other.capacity;
		this->allocator = other.allocator;

		other.data = 0;
		other.length = 0;
		other.capacity = 0;
	}

	inline StringBuffer(const StringBuffer& other) {
		if (other.length == 0) return;
		this->reserve(other.length);
		this->allocator = other.allocator;
		this->length = other.length;

		memcpy(this->data, other.data, other.length);
		data[length] = '\0';
	}

	inline StringBuffer& operator=(StringBuffer&& other) {
		this->~StringBuffer();

		this->length = other.length;
		this->allocator = other.allocator;
		this->capacity = other.capacity;
		this->data = other.data;

		other.length = 0;
		other.data = NULL;
		other.capacity = 0;

		return *this;
	}

	inline StringBuffer& operator=(StringView other) {
		if (*this == other) return *this;
		
		this->reserve(other.length);
		this->length = other.length;

		memcpy(this->data, other.data, this->length);
		data[length] = '\0';

		return *this;
	}
	
	inline StringBuffer& operator=(const StringBuffer& other) {
		if (*this == other) return *this;

		this->reserve(other.length);
		this->length = other.length;

		memcpy(this->data, other.data, sizeof(char) * length);
		data[length] = '\0';

		return *this;
	}

	inline StringBuffer& operator+=(const StringView other) {
		if (other.length == 0) return *this;
		if (length + other.length > capacity) {
			this->reserve(glm::max(this->length + other.length, this->capacity * 2));
		}

		memcpy(this->data + this->length, other.data, other.length);
		this->length += other.length;
		data[length] = '\0';

		return *this;
	}

	inline StringView view() {
		return { data == NULL ? "" : data, length };
	}

	inline StringBuffer operator+(StringView other) {
		StringBuffer self;
		self.reserve(this->length + other.length);
		self += this->view();
		self += other;

		return self;
	}

	inline const char* c_str() {
		return view().c_str();
	}

	inline bool starts_with(StringView pre) {
		return view().starts_with(pre);
	}

	inline bool starts_with_ignore_case(StringView pre) {
		return view().starts_with_ignore_case(pre);
	}

	inline StringBuffer lower() {
		StringBuffer new_str;
		new_str.reserve(length);

		for (int i = 0; i < length; i++) {
			new_str.data[i] = to_lower_case(data[i]);
		}

		return new_str;
	}

	inline bool ends_with(StringView pre) {
		return view().ends_with(pre);
	}

	inline int find_last_of(char c) {
		return view().find_last_of(c);
	}

	inline int find(char c) {
		return view().find(c);
	}

	inline int operator==(StringView other) {
		return view() == other;
	}

	inline int operator!=(StringView other) {
		return view() != other;
	}

	inline StringView sub(unsigned int start, unsigned int end) {
		return view().sub(start, end);
	}

	inline vec_iterator<char> begin() {
		return vec_iterator<char>(this->data, 0);
	}

	inline vec_iterator<char> end() {
		return vec_iterator<char>(this->data, length);
	}

	inline vec_iterator<const char> begin() const {
		return vec_iterator<const char>(this->data, 0);
	}

	inline vec_iterator<const char> end() const {
		return vec_iterator<const char>(this->data, length);
	}

	inline char operator[](unsigned int index) {
		assert(index < this->length);
		return data[index];
	}

	inline StringBuffer() {};
	
	inline ~StringBuffer() {
		this->allocator->deallocate(this->data);
	}

	void hash();
};