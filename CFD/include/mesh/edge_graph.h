#pragma once

#include "cfd_core.h"
#include "core/container/slice.h"

struct EdgeGraph {
	struct Slice {
		uint offset;
		uint count;
	};

	edge_handle* edges;
	Slice* id_to_neighbors;

	slice<edge_handle> neighbors(vertex_handle v) {
		Slice& slice = id_to_neighbors[v.id];
		return { edges + slice.offset, slice.count };
	}
};

struct SurfaceTriMesh;

EdgeGraph build_edge_graph(SurfaceTriMesh& surface);