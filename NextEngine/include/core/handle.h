#pragma once

#include "reflection/reflection.h"

constexpr unsigned int INVALID_HANDLE = 0;

template<typename T>
struct Handle {
	unsigned int id = INVALID_HANDLE;

	REFLECT()
};

