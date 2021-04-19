#pragma once

#include "core/core.h"
#include "mesh.h"

struct VertGraph {
	struct Slice {
		uint offset;
		uint count;
	};

	vertex_handle* neighbor_vertices;
	Slice* id_to_neighbors;

	slice<vertex_handle> neighbors(vertex_handle v);
	array<20, vertex_handle> neighbors(vertex_handle va, vertex_handle vb);
	array<10, vertex_handle> neighbors(vertex_handle a, vertex_handle b, vertex_handle c);
};