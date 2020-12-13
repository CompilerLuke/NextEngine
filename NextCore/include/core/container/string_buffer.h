#pragma once

#include "core/memory/allocator.h"
#include <string.h>
#include "core/container/string_view.h"

struct string_buffer {
	Allocator* allocator = &default_allocator;
	char* data = (char*)"";
	unsigned int capacity = 0;
	unsigned int length = 0;

	inline unsigned int size() {
		return length;
	}

	inline void reserve(uint amount) {
		if (amount > capacity) {
			char* data = (char*)allocator->allocate(sizeof(char) * ((size_t)amount + 1));

			if (this->length > 0) {
				memcpy(data, this->data, sizeof(char) * ((size_t)length + 1));
				allocator->deallocate(this->data);
			}

			this->data = data;
			this->capacity = amount;
		}
	}

	inline string_buffer(const char* str) {
		uint length = (uint)strlen(str);
		if (length == 0) return;
		this->reserve(length);
		this->length = length;
		memcpy(this->data, str, sizeof(char) * ((size_t)length + 1));
	}

	inline string_buffer(string_view str) {
		this->reserve(str.length);
		this->length = str.length;
		memcpy(this->data, str.data, length);
		data[length] = '\0';
	}

	inline string_buffer(string_buffer&& other) {
		this->data = other.data;
		this->length = other.length;
		this->capacity = other.capacity;
		this->allocator = other.allocator;

		other.data = 0;
		other.length = 0;
		other.capacity = 0;
	}

	inline string_buffer(const string_buffer& other) {
		if (other.length == 0) return;

		this->allocator = other.allocator;
		this->reserve(other.length);		
		this->length = other.length;	

		memcpy(this->data, other.data, other.length);
		data[length] = '\0';
	}

	inline string_buffer& operator=(string_buffer&& other) {
		this->length = other.length;
		this->allocator = other.allocator;
		this->capacity = other.capacity;
		this->data = other.data;

		other.length = 0;
		other.data = NULL;
		other.capacity = 0;

		return *this;
	}

	inline string_buffer& operator=(string_view other) {
		//if (*this == other) return *this;
        if (other.length == 0) {
            length = 0;
            if (capacity) data[0] = '\0';
            return *this;
        }
		
		this->reserve(other.length);
		this->length = other.length;

		memcpy(this->data, other.data, this->length);
		data[length] = '\0';

		return *this;
	}
    
    inline string_buffer& operator=(const char* other) {
        return *this = (string_view)other;
    }

    /*
	inline string_buffer& operator=(const string_buffer& other) {
		if (*this == other) return *this;

		this->reserve(other.length);
		this->length = other.length;

		memcpy(this->data, other.data, this->length);
		data[length] = '\0';

		return *this;
	}*/

	inline string_buffer& operator+=(const string_view other) {
		if (other.length == 0) return *this;
		if (length + other.length > capacity) {
			this->reserve(max(this->length + other.length, this->capacity * 2));
		}

		memcpy(this->data + this->length, other.data, other.length);
		this->length += other.length;
		data[length] = '\0';

		return *this;
	}

	inline string_view view() const {
		return { data, length };
	}

	inline string_buffer operator+(string_view other) {
		string_buffer self;
		self.reserve(this->length + other.length);
		self += this->view();
		self += other;

		return self;
	}

	inline const char* c_str() {
		return view().c_str();
	}

	inline bool starts_with(string_view pre) {
		return view().starts_with(pre);
	}

	inline bool starts_with_ignore_case(string_view pre) {
		return view().starts_with_ignore_case(pre);
	}

	inline string_buffer lower() {
		string_buffer new_str;
		new_str.reserve(length);

		for (uint i = 0; i < length; i++) {
			new_str.data[i] = to_lower_case(data[i]);
		}

		return new_str;
	}

	inline bool ends_with(string_view pre) {
		return view().ends_with(pre);
	}

	inline int find_last_of(char c) {
		return view().find_last_of(c);
	}

	inline int find(char c) {
		return view().find(c);
	}

	inline int operator==(string_view other) const {
		return view() == other;
	}

	inline int operator!=(string_view other) const {
		return view() != other;
	}

	inline string_view sub(unsigned int start, unsigned int end) {
		return view().sub(start, end);
	}

	inline char* begin() {
		return this->data;
	}

	inline char* end() {
		return this->data + length;
	}

	inline const char* begin() const {
		return this->data;
	}

	inline const char* end() const {
		return this->data + length;
	}

	inline char operator[](unsigned int index) {
		assert(index < this->length);
		return data[index];
	}

	inline string_buffer() {};
	
	inline ~string_buffer() {
		if (this->capacity > 0) this->allocator->deallocate(this->data);
	}

	operator string_view() const&  {
		return { data, length };
	}

	void hash();
};
