#pragma once

#include "core/core.h"

using tri_handle = uint;
using edge_handle = uint;

struct vertex_handle {
	int id = -1;
};

#define TRI(tri) (uint)(tri/3)*3
#define TRI_EDGE(tri) tri%3