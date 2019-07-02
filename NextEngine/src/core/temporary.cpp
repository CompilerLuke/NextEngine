#include "stdafx.h"
#include "core/temporary.h"

void TemporaryAllocator::clear() {
	this->occupied = 0;
}

TemporaryAllocator::TemporaryAllocator(size_t max_size) {
	this->occupied = 0;
	this->max_size = max_size;
	this->memory = ALLOC_ARRAY(char, max_size);
}

TemporaryAllocator::~TemporaryAllocator() {
	FREE_ARRAY(this->memory, max_size);
}

void* TemporaryAllocator::allocate(size_t size) {
	auto occupied = this->occupied;
	this->occupied += size;
	if (this->occupied > this->max_size) {
		throw "Temporary allocator out of memory";
	}
	return this->memory + occupied;
}

