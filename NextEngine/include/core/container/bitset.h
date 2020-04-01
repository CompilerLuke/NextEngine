#pragma once

#include "core/core.h"

inline bool get_b

inline void set_bit(uint* bitset, uint i) {
	bitset[i / 32] |= 1 << (i % 32);
}