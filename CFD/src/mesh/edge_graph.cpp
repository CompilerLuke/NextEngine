
#include "mesh/edge_graph.h"
#include "mesh/surface_tet_mesh.h"
#include "core/memory/linear_allocator.h"

EdgeGraph build_edge_graph(SurfaceTriMesh& surface) {
	EdgeGraph::Slice* neighbors = TEMPORARY_ZEROED_ARRAY(EdgeGraph::Slice, surface.positions.length);

	for (tri_handle i : surface) {
		for (uint j = 0; j < 3; j++) {
			vertex_handle v0 = surface.indices[i + j];
			vertex_handle v1 = surface.indices[i + (j + 1) % 3];

			if (v0.id > v1.id) continue;

			neighbors[v0.id].count++;
			neighbors[v1.id].count++;
		}
	}

	uint offset = 0;
	for (uint i = 0; i < surface.positions.length; i++) {
		neighbors[i].offset = offset;
		offset += neighbors[i].count;
		neighbors[i].count = 0;
	}

	edge_handle* neighbor_vertices = TEMPORARY_ZEROED_ARRAY(edge_handle, offset);

	for (tri_handle i : surface) {
		for (uint j = 0; j < 3; j++) {
			vertex_handle v0 = surface.indices[i + j];
			vertex_handle v1 = surface.indices[i + (j + 1) % 3];

			if (v0.id > v1.id) continue;

			neighbor_vertices[neighbors[v0.id].offset + neighbors[v0.id].count++] = surface.edges[i + j];

			assert(surface.indices[surface.edges[i + j]].id == v1.id);

			neighbor_vertices[neighbors[v1.id].offset + neighbors[v1.id].count++] = i + j;
		}
	}

	return { neighbor_vertices, neighbors };
}