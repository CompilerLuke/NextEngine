#pragma once

#include "core/allocator.h"
#include <memory.h>
#include <string.h>
#include "core/vector.h"
#include <algorithm>

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
			char* data = (char*)allocator->allocate(sizeof(char) * (this->length + 1));
			
			if (this->data) memcpy(data, this->data, sizeof(char) * (this->length + 1));

			allocator->deallocate(this->data);
		}
	}

	inline StringBuffer(const char* str) {
		unsigned int length = strlen(str);
		this->reserve(length);
		memcpy(this->data, str, sizeof(char) * (length + 1));
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
		this->reserve(other.length);
		this->allocator = other.allocator;
		this->length = other.length;

		memcpy(this->data, other.data, other.length + 1);
	}

	inline StringBuffer& operator=(const StringBuffer& other) {
		this->allocator = other.allocator;
		this->reserve(other.length);
		this->length = other.length;

		memcpy(this->data, other.data, sizeof(char) * (this->length + 1));
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

	inline StringBuffer& append(const StringBuffer& other) {
		if (length + other.length > capacity) {
			this->reserve(std::max(this->length + other.length, this->capacity * 2));
		}

		memcpy(this->data + this->length, other.data, other.length + 1);
	}

	inline StringBuffer& append(const char* other) {
		unsigned int length = strlen(other);
		
		if (length + length > capacity) {
			this->reserve(std::max(this->length + length, this->capacity * 2));
		}

		memcpy(this->data + this->length, other, length + 1);
	}

	inline const char* c_str() {
		return this->data;
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

	inline StringBuffer() {};
	
	inline ~StringBuffer() {
		this->allocator->deallocate(this->data);
	}
};