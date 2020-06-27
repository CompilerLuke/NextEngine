#include "core/memory/linear_allocator.h"
#include <memory>

void LinearAllocator::clear() {
	this->occupied = 0;
}

LinearAllocator::LinearAllocator(size_t max_size) {
	this->occupied = 0;
	this->max_size = max_size;
	this->memory = ALLOC_ARRAY(char, max_size);
}

LinearAllocator::~LinearAllocator() {
	FREE_ARRAY(this->memory, max_size);
}

void* LinearAllocator::allocate(size_t size) {
	u64 offset = aligned_incr(&occupied, size, 16);

	if (occupied > max_size) {
		throw "Temporary allocator out of memory";
	}

	return memory + offset;
}

void LinearAllocator::reset(size_t occupied) {
	this->occupied = occupied;
}

LinearRegion::LinearRegion(LinearAllocator& allocator) : allocator(allocator) {
	watermark = allocator.occupied;
}

LinearRegion::~LinearRegion() {
	allocator.reset(watermark);
}