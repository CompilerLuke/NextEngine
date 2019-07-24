#pragma once

#include "reflection/reflection.h"
#include <cstdint>
#include "core/string_buffer.h"

struct SerializerBuffer { 
	Allocator* allocator = &default_allocator;
	char* data = NULL;
	unsigned int index = 0; //also size
	unsigned int capacity = 0;

	void* pointer_to_n_bytes(unsigned int);
	void write_byte(uint8_t);
	void write_int(int32_t);
	void write_long(int64_t);
	void write_float(float);
	void write_double(double);
	void write_struct(reflect::TypeDescriptor_Struct*, void*);
	void write_union(reflect::TypeDescriptor_Union*, void*);
	void write_string(StringBuffer&);
	void write_array(reflect::TypeDescriptor_Vector*, void*);
	void write(reflect::TypeDescriptor*, void*);

	~SerializerBuffer();
};

struct DeserializerBuffer {
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
	void read_struct(reflect::TypeDescriptor_Struct*, void*);
	void read_union(reflect::TypeDescriptor_Union*, void*);
	void read_string(StringBuffer&);
	void read_array(reflect::TypeDescriptor_Vector*, void*);
	void read(reflect::TypeDescriptor*, void*);
};