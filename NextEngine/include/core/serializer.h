#pragma once

#include "core/reflection.h"
#include <cstdint>
#include "core/container/string_buffer.h"
#include "core/container/sstring.h"
#include "core/container/array.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct SerializerBuffer {
	char* data;
	uint index = 0;
	uint capacity;
};

struct DeserializerBuffer {
	char* data;
	uint index;
	uint length;
};
 
inline void write_to_buffer(SerializerBuffer& buffer, void* ptr, u64 size) {
	memcpy(buffer.data + buffer.index, ptr, size);
	buffer.index += size;
}

inline void read_from_buffer(DeserializerBuffer& buffer, void* ptr, u64 size) {
	memcpy(ptr, buffer.data + buffer.index, size);
	buffer.index += size;
}

template<int N, typename T>
void write_to_buffer(SerializerBuffer& buffer, T arr[N]) {
	for (uint i = 0; i < N; i++) {
		write_to_buffer(buffer, arr[i]);
	}
}

template<int N, typename T>
void read_from_buffer(SerializerBuffer& buffer, T arr[N]) {
	for (uint i = 0; i < N; i++) {
		read_from_buffer(buffer, arr[i]);
	}
}


template<int N, typename T>
void write_to_buffer(SerializerBuffer& buffer, array<N, T>& arr) {
	write_to_buffer(buffer, arr.length);
	for (uint i = 0; i < arr.length; i++) {
		write_to_buffer(buffer, arr[i]);
	}
}

template<int N, typename T>
void read_from_buffer(SerializerBuffer& buffer, array<N, T>& arr) {
	read_from_buffer(buffer, arr.length);
	arr.resize(arr.length);
	for (uint i = 0; i < arr.length; i++) {
		read_from_buffer(buffer, arr[i]);
	}
}

template<typename T>
void write_to_buffer(SerializerBuffer& buffer, vector<T>& arr) {
	write_int(buffer, arr.length);
	for (uint i = 0; i < arr.length; i++) {
		write_to_buffer(buffer, arr[i]);
	}
}

template<typename T>
void read_from_buffer(SerializerBuffer& buffer, vector<T>& arr) {
	read_from_buffer(buffer, arr.length);
	arr.reserve(arr.length);
	for (uint i = 0; i < arr.length; i++) {
		write_to_buffer(buffer, arr[i]);
	}
}

inline void write_to_buffer(SerializerBuffer& buffer, char value) {
	write_to_buffer(buffer, &value, sizeof(char));
}

inline void read_from_buffer(DeserializerBuffer& buffer, char& value) {
	read_from_buffer(buffer, &value, sizeof(char));
}

inline void write_to_buffer(SerializerBuffer& buffer, uint value) {
	write_to_buffer(buffer, &value, sizeof(uint));
}

inline void read_from_buffer(DeserializerBuffer& buffer, uint* value) {
	read_from_buffer(buffer, &value, sizeof(uint));
}

inline void write_to_buffer(SerializerBuffer& buffer, int value) {
	write_to_buffer(buffer, &value, sizeof(int));
}

inline void write_to_buffer(SerializerBuffer& buffer, u64 value) {
	write_to_buffer(buffer, &value, sizeof(u64));
}

inline void write_to_buffer(SerializerBuffer& buffer, i64 value) {
	write_to_buffer(buffer, &value, sizeof(i64));
}

const int float_precision = 1000;
const int double_precision = 100000;

inline void write_to_buffer(SerializerBuffer& buffer, float value) {
	write_to_buffer(buffer, (int)(value * float_precision));
}

inline void write_to_buffer(SerializerBuffer& buffer, f64 value) {
	write_to_buffer(buffer, (int)(value * double_precision));
}

inline void write_to_buffer(SerializerBuffer& buffer, glm::vec2 value) {
	write_to_buffer(buffer, value.x);
	write_to_buffer(buffer, value.y);
}

inline void write_to_buffer(SerializerBuffer& buffer, glm::vec3 value) {
	write_to_buffer(buffer, value.x);
	write_to_buffer(buffer, value.y);
	write_to_buffer(buffer, value.z);
}

inline void write_to_buffer(SerializerBuffer& buffer, glm::quat value) {
	write_to_buffer(buffer, value.x);
	write_to_buffer(buffer, value.y);
	write_to_buffer(buffer, value.z);
	write_to_buffer(buffer, value.w);
}

inline void write_to_buffer(SerializerBuffer& buffer, const glm::mat4& value) {
	for (uint x = 0; x < 16; x++) {
		for (uint y = 0; y < 4; y++) {
			write_to_buffer(buffer, value[x][y]);
		}
	}
}

inline void write_to_buffer(SerializerBuffer& buffer, const sstring& str) {
	uint length = str.length();
	
	write_to_buffer(buffer, length);
	for (uint i = 0; i < length; i++) {
		write_to_buffer(buffer, str.data[i]);
	}
}

inline void write_to_buffer(SerializerBuffer& buffer, const string_buffer& str) {
	write_to_buffer(buffer, str.length);
	for (uint i = 0; i < str.length; i++) {
		write_to_buffer(buffer, str.data[i]);
	}
}

/*

/*
struct ENGINE_API SerializerBuffer {
	Allocator* allocator = &default_allocator;
	char* data = NULL;
	unsigned int index = 0; //also size
	unsigned int capacity = 0;

	void* pointer_to_n_bytes(unsigned int);
	void write_byte(uint8_t);
	void write_int(int32_t);
	void write_uint(uint32_t);
	void write_long(int64_t);
	void write_float(float);
	void write_double(double);
	void write_struct(refl::Struct*, void*);
	//void write_union(refl::TypeDescriptor_Union*, void*);
	void write_string(string_buffer&);
	void write_string(sstring&);
	void write_array(refl::Array*, void*);
	void write(refl::Type*, void*);

	~SerializerBuffer();
};

struct ENGINE_API DeserializerBuffer {
	DeserializerBuffer();
	DeserializerBuffer(const char* data, unsigned int length);

	const char* data;
	unsigned int index = 0;
	unsigned int length;

	const void* pointer_to_n_bytes(unsigned int);
	uint8_t read_byte();
	int32_t read_int();
	int64_t read_long();
	float read_float();
	double read_double();
	void read_struct(refl::Struct*, void*);
	//void read_union(refl::TypeDescriptor_Union*, void*);
	void read_string(string_buffer&);
	void read_string(sstring&);
	void read_array(refl::Array*, void*);
	void read(refl::Type*, void*);
};*/

