#include "stdafx.h"
#include "serialization/serializer.h"
#include "core/vfs.h"
#include "ecs/ecs.h"
#include <WinSock2.h>

#pragma comment(lib, "Ws2_32.lib")

#define u32_to_network htonl
#define u32_to_local ntohl
#define u64_to_network htonll
#define u64_to_local ntohll

void* SerializerBuffer::pointer_to_n_bytes(unsigned int num) {
	if (index + num > capacity) {
		this->capacity = capacity == 0 ? 128 : capacity * 2;
		if (this->capacity < index + num) capacity = index + num;

		char* data = (char*)allocator->allocate(this->capacity);
		memcpy(data, this->data, index);
		allocator->deallocate(this->data);
		this->data = data;

	}

	void* ptr = this->data + index;
	index += num;
	return ptr;
}

const void* DeserializerBuffer::pointer_to_n_bytes(unsigned int num) {
	const void* ptr = this->data + index;
	index += num;
	assert(index <= length);

	return ptr;
}

void SerializerBuffer::write_struct(reflect::TypeDescriptor_Struct* type, void* ptr) {
	for (auto& member : type->members) {
		write(member.type, (char*)ptr + member.offset);
	}
}

void DeserializerBuffer::read_struct(reflect::TypeDescriptor_Struct* type, void* ptr) {
	for (auto& member : type->members) {
		read(member.type, (char*)ptr + member.offset);
	}
}

void SerializerBuffer::write_union(reflect::TypeDescriptor_Union* type, void* ptr) {
	int tag = *(int*)((char*)ptr + type->tag_offset);

	write_int(tag);

	for (auto& member : type->members) {
		write(member.type, (char*)ptr + member.offset);
	}

	auto& union_case = type->cases[tag];
	write(union_case.type, (char*)ptr + union_case.offset);
}

#include "graphics/materialSystem.h"
#include "logger/logger.h"

void DeserializerBuffer::read_union(reflect::TypeDescriptor_Union* type, void* ptr) {
	int tag = read_int();
	*(int*)((char*)ptr + type->tag_offset) = (char)tag;

	Param* mat = (Param*)ptr;

	for (auto& member : type->members) {
		read(member.type, (char*)ptr + member.offset);
	}

	auto& union_case = type->cases[tag];
	read(union_case.type, (char*)ptr + union_case.offset);

	log("here");
}

void SerializerBuffer::write_byte(uint8_t byte) {
	*(uint8_t*)pointer_to_n_bytes(1) = byte;
}

uint8_t DeserializerBuffer::read_byte() {
	return *(uint8_t*)pointer_to_n_bytes(1);
}

void SerializerBuffer::write_int(int32_t value) {
	auto ptr = (uint32_t*)pointer_to_n_bytes(4);
	*ptr = u32_to_network(*(uint32_t*)&value);
}

int32_t DeserializerBuffer::read_int() {
	auto value = u32_to_local(*(uint32_t*)pointer_to_n_bytes(4));
	return *(int32_t*)&value;
}

void SerializerBuffer::write_long(int64_t value) {
	auto ptr = (uint64_t*)pointer_to_n_bytes(8);
	*ptr = u64_to_network(*(uint64_t*)&value);
}

int64_t DeserializerBuffer::read_long() {
	auto value = u64_to_local(*(uint64_t*)pointer_to_n_bytes(4));
	return *(int64_t*)&value;
}

constexpr unsigned int float_precision = 10000;
constexpr unsigned double_precision = 100000000;

void SerializerBuffer::write_float(float value) {
	write_int((int32_t)(value * float_precision));
}

float DeserializerBuffer::read_float() {
	return (float)read_int() / float_precision;
}

void SerializerBuffer::write_double(double value) {
	write_long((int64_t)(value * double_precision));
}

double DeserializerBuffer::read_double() {
	return (double)read_long() * double_precision;
}

void SerializerBuffer::write_array(reflect::TypeDescriptor_Vector* type, void* ptr) {
	vector<char>* arr = (vector<char>*)ptr;

	write_int(arr->length);
	for (int i = 0; i < arr->length; i++) {
		write(type->itemType, (char*)arr->data + (i * type->itemType->size));
	}
}

void DeserializerBuffer::read_array(reflect::TypeDescriptor_Vector* type, void* ptr) {
	vector<char>* arr = (vector<char>*)ptr;

	unsigned int length = (unsigned int)read_int();
	arr->length = length;
	arr->reserve(length * type->itemType->size);
	arr->capacity = length;

	for (int i = 0; i < arr->length; i++) {
		read(type->itemType, (char*)arr->data + (i * type->itemType->size));
	}
}

void SerializerBuffer::write_string(StringBuffer& str) {
	write_int(str.size());

	for (int i = 0; i < str.size(); i++) {
		write_byte(str[i]);
	}
}

void DeserializerBuffer::read_string(StringBuffer& str) {
	unsigned int length = (unsigned int)this->read_int();
	str.reserve(length);
	str.length = length;

	for (int i = 0; i < length; i++) {
		str.data[i] = this->read_byte();
	}

	str.data[length] = '\0';
}


void SerializerBuffer::write(reflect::TypeDescriptor* type, void* ptr) {
	if (type->kind == reflect::Struct_Kind) write_struct((reflect::TypeDescriptor_Struct*)type, ptr);
	else if (type->kind == reflect::Union_Kind) write_union((reflect::TypeDescriptor_Union*)type, ptr);
	else if (type->kind == reflect::Bool_Kind) write_byte(*(bool*)ptr);
	else if (type->kind == reflect::Float_Kind) write_float(*(float*)ptr);
	else if (type->kind == reflect::Int_Kind) write_int(*(int*)ptr);
	else if (type->kind == reflect::Unsigned_Int_Kind) write_int(*(unsigned int*)ptr);
	else if (type->kind == reflect::StringBuffer_Kind) write_string(*(StringBuffer*)ptr);
	else if (type->kind == reflect::Vector_Kind) write_array((reflect::TypeDescriptor_Vector*)type, ptr);
	else throw "Unexpected type";
}

void DeserializerBuffer::read(reflect::TypeDescriptor* type, void* ptr) {
	if (type->kind == reflect::Struct_Kind) read_struct((reflect::TypeDescriptor_Struct*)type, ptr);
	else if (type->kind == reflect::Union_Kind) read_union((reflect::TypeDescriptor_Union*)type, ptr);
	else if (type->kind == reflect::Bool_Kind) *(bool*)ptr = read_byte();
	else if (type->kind == reflect::Float_Kind) *(float*)ptr = read_float();
	else if (type->kind == reflect::Int_Kind) *(int*)ptr = read_int();
	else if (type->kind == reflect::Unsigned_Int_Kind) *(unsigned int*)ptr = read_int();
	else if (type->kind == reflect::StringBuffer_Kind) read_string(*(StringBuffer*)ptr);
	else if (type->kind == reflect::Vector_Kind) read_array((reflect::TypeDescriptor_Vector*)type, ptr);
	else throw "Unexpected type";
}

SerializerBuffer::~SerializerBuffer() {
	allocator->deallocate(data);
}

DeserializerBuffer::DeserializerBuffer(const char* data, unsigned int length) : data(data), length(length) {

}