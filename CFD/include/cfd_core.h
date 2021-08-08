#pragma once

#include "core/core.h"

using tri_handle = uint;
using edge_handle = uint;

struct vertex_handle {
	int id = -1;
};

inline u64 hash_func(vertex_handle v) { return v.id; }
inline bool operator==(vertex_handle a, vertex_handle b) { return a.id == b.id; }
inline bool operator!=(vertex_handle a, vertex_handle b) { return a.id != b.id; }