#pragma once

#include <assert.h>
#include "core/memory/linear_allocator.h"
#include "core/container/string_view.h"

using string = string_view;

template<typename T>
T* alloc(LinearAllocator* allocator, uint count = 1) {
	return new (allocator->allocate(sizeof(T) * count)) T[count]();
}

inline void to_cstr(string str, char* buffer, uint length) {
	assert(str.length < length);
	memcpy(buffer, str.data, str.length);
	buffer[str.length] = '\0';
}