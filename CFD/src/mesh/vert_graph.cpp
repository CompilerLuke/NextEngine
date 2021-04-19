#include "mesh/vert_graph.h"

slice<vertex_handle> VertGraph::neighbors(vertex_handle v) {
	VertGraph::Slice& slice = id_to_neighbors[v.id];
	return { neighbor_vertices + slice.offset, slice.count };
}

array<20, vertex_handle> VertGraph::neighbors(vertex_handle va, vertex_handle vb) {
	slice<vertex_handle> neigh_a = neighbors(va);
	slice<vertex_handle> neigh_b = neighbors(vb);

	array<20, vertex_handle> shared;

	uint i = 0;
	uint j = 0;
	while (i < neigh_a.length && j < neigh_b.length) {
		vertex_handle a = neigh_a[i];
		vertex_handle b = neigh_b[j];

		if (a.id > b.id) j++;
		else if (b.id > a.id) i++;
		else {
			i++, j++;
			shared.append(a);
		}
	}

	return shared;
}

array<10, vertex_handle> VertGraph::neighbors(vertex_handle a, vertex_handle b, vertex_handle c) {
	slice<vertex_handle> neigh_a = neighbors(a);
	slice<vertex_handle> neigh_b = neighbors(b);
	slice<vertex_handle> neigh_c = neighbors(c);

	array<10, vertex_handle> shared;
	uint i = 0;
	uint j = 0;
	uint k = 0;

	while (i < neigh_a.length && j < neigh_b.length && k < neigh_c.length) {
		vertex_handle a = neigh_a[i];
		vertex_handle b = neigh_b[j];
		vertex_handle c = neigh_c[k];

		if (a.id == b.id && b.id == c.id) {
			shared.append(a);
			i++, j++, k++;
		}
		else if (a.id > b.id) j++;
		else if (b.id > c.id) k++;
		else i++;
	}

	return shared;
}