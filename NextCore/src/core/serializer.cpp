#include "core/serializer.h"
#include "core/io/vfs.h"

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

void SerializerBuffer::write_struct(refl::Struct* type, void* ptr) {
	for (auto& member : type->fields) {
		write(member.type, (char*)ptr + member.offset);
	}
}

void DeserializerBuffer::read_struct(refl::Struct* type, void* ptr) {
	for (auto& member : type->fields) {
		read(member.type, (char*)ptr + member.offset);
	}
}

/*
void SerializerBuffer::write_union(refl::TypeDescriptor_Union* type, void* ptr) {
	int tag = *(int*)((char*)ptr + type->tag_offset);

	write_int(tag);

	for (auto& member : type->members) {
		write(member.type, (char*)ptr + member.offset);
	}

	auto& union_case = type->cases[tag];
	write(union_case.type, (char*)ptr + union_case.offset);
}*/

/*
void DeserializerBuffer::read_union(reflect::TypeDescriptor_Union* type, void* ptr) {
	int tag = read_int();
	*(int*)((char*)ptr + type->tag_offset) = (char)tag;

	for (auto& member : type->members) {
		read(member.type, (char*)ptr + member.offset);
	}

	auto& union_case = type->cases[tag];
	read(union_case.type, (char*)ptr + union_case.offset);
}*/

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

void SerializerBuffer::write_array(refl::Array* type, void* ptr) {
	vector<char>* arr = (vector<char>*)ptr;

	write_int(arr->length);
	for (int i = 0; i < arr->length; i++) {
		write(type->element, (char*)arr->data + (i * type->element->size));
	}
}

void DeserializerBuffer::read_array(refl::Array* type, void* ptr) {
	vector<char>* arr = (vector<char>*)ptr;

	unsigned int length = (unsigned int)read_int();
	arr->length = length;
	arr->reserve(length * type->element->size);
	arr->capacity = length;

	for (int i = 0; i < arr->length; i++) {
		read(type->element, (char*)arr->data + (i * type->element->size));
	}
}

void SerializerBuffer::write_string(string_buffer& str) {
	write_int(str.size());

	for (int i = 0; i < str.size(); i++) {
		write_byte(str[i]);
	}
}

void SerializerBuffer::write_string(sstring& str) {
	write_int(str.length());

	for (int i = 0; i < str.length(); i++) {
		write_byte(str[i]);
	}
}


void DeserializerBuffer::read_string(string_buffer& str) { //todo merge into same function
	unsigned int length = (unsigned int)this->read_int();
	str.reserve(length);
	str.length = length;

	for (int i = 0; i < length; i++) {
		str.data[i] = this->read_byte();
	}

	str.data[length] = '\0';
}

void DeserializerBuffer::read_string(sstring& str) {
	uint length = (uint)this->read_int();
	assert(length < sstring::N);	
	str.length(length);

	for (int i = 0; i < length; i++) {
		str.data[i] = this->read_byte();
	}

	str.data[length] = '\0';

}


void SerializerBuffer::write(refl::Type* type, void* ptr) {
	using refl::Type;
	
	if (type->type == refl::Type::Struct) write_struct((refl::Struct*)type, ptr);
	//else if (type->type == refl::Type::Union) write_union((reflect::TypeDescriptor_Union*)type, ptr);
	else if (type->type == Type::Bool) write_byte(*(bool*)ptr);
	else if (type->type == Type::Float) write_float(*(float*)ptr);
	else if (type->type == Type::Int) write_int(*(int*)ptr);
	else if (type->type == Type::UInt) write_int(*(unsigned int*)ptr);
	else if (type->type == Type::StringBuffer) write_string(*(string_buffer*)ptr);
	else if (type->type == Type::SString) write_string(*(sstring*)ptr);
	else if (type->type == Type::Array) write_array((refl::Array*)type, ptr);
	//else if (type->kind == Type::Enum) write_int(*(int*)ptr);
	else throw "Unexpected type";
}

void DeserializerBuffer::read(refl::Type* type, void* ptr) {
	using refl::Type;
	
	if (type->type == refl::Type::Struct) read_struct((refl::Struct*)type, ptr);
	//else if (type->kind == reflect::Union_Kind) read_union((reflect::TypeDescriptor_Union*)type, ptr);
	else if (type->type == Type::Bool) *(bool*)ptr = read_byte();
	else if (type->type == Type::Float) *(float*)ptr = read_float();
	else if (type->type == Type::Int) *(int*)ptr = read_int();
	else if (type->type == Type::UInt) *(int*)ptr = read_int();
	else if (type->type == Type::StringBuffer) read_string(*(string_buffer*)ptr);
	else if (type->type == Type::SString) read_string(*(sstring*)ptr);
	else if (type->type == Type::Array) read_array((refl::Array*)type, ptr);
	//else if (type->kind == reflect::Enum_Kind) *(int*)ptr = read_int();
	else throw "Unexpected type";
}

SerializerBuffer::~SerializerBuffer() {
	allocator->deallocate(data);
}

DeserializerBuffer::DeserializerBuffer(const char* data, unsigned int length) : data(data), length(length) {

}

DeserializerBuffer::DeserializerBuffer() {}