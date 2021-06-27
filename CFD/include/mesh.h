#pragma once

#include "core/core.h"
#include "core/container/vector.h"
#include "core/container/array.h"
#include "core/math/vec3.h"
#include "core/math/aabb.h"
#include "cfd_core.h"

struct polygon_handle {
	int id = -1;
};

struct cell_handle {
	int id = -1;
};

struct EdgeSet {
	vertex_handle a;
	vertex_handle b;

	inline EdgeSet() {}
	inline EdgeSet(vertex_handle a, vertex_handle b) {
		if (a.id > b.id) {
			this->a = b;
			this->b = a;
		}
		else {
			this->a = a;
			this->b = b;
		}
	}
};

inline u64 hash_func(EdgeSet edge) {
	return (u64)edge.a.id << 32 | edge.b.id;
}

inline bool operator==(EdgeSet a, EdgeSet b) {
	return a.a.id == b.a.id && a.b.id == b.b.id;
}


inline void swap_vertex(vertex_handle& a, vertex_handle& b) {
	vertex_handle tmp = a;
	a = b;
	b = tmp;
}

inline void sort3_vertices(vertex_handle verts[3]) {
	if (verts[0].id > verts[1].id) swap_vertex(verts[0], verts[1]);
	if (verts[1].id > verts[2].id) swap_vertex(verts[1], verts[2]);
	if (verts[0].id > verts[1].id) swap_vertex(verts[0], verts[1]);
}

inline void sort4_vertices(vertex_handle verts[4]) {
    if (verts[0].id > verts[1].id) swap_vertex(verts[0], verts[1]);
    if (verts[2].id > verts[3].id) swap_vertex(verts[2], verts[3]);
    if (verts[0].id > verts[2].id) swap_vertex(verts[2], verts[3]);
    if (verts[1].id > verts[3].id) swap_vertex(verts[1], verts[3]);
    if (verts[1].id > verts[2].id) swap_vertex(verts[1], verts[2]);
}

struct TriangleFaceSet {
	vertex_handle verts[3];

	inline TriangleFaceSet() {}
	inline TriangleFaceSet(const vertex_handle v[3]) {
		this->verts[0] = v[0];
		this->verts[1] = v[1];
		this->verts[2] = v[2];
		sort3_vertices(this->verts);
	}
    inline TriangleFaceSet(vertex_handle a, vertex_handle b, vertex_handle c) {
        this->verts[0] = a;
        this->verts[1] = b;
        this->verts[2] = c;
        sort3_vertices(this->verts);
    }

	inline bool operator==(const TriangleFaceSet& other) const {
		return memcmp(verts, other.verts, sizeof(vertex_handle) * 3) == 0;
	}
};

inline u64 hash_func(const TriangleFaceSet& set) {
	return (u64)set.verts[0].id | (u64)set.verts[1].id << 21 | (u64)set.verts[2].id << 42;
}

namespace std {

	template <>
	struct hash<TriangleFaceSet>
	{
		std::size_t operator()(const TriangleFaceSet& set) const
		{
			return (u64)set.verts[0].id | (u64)set.verts[1].id << 21 | (u64)set.verts[2].id << 42;
		}
	};

}

struct Boundary {
	cell_handle cell;
	vertex_handle vertices[4];
};

//enum FaceSide {
//	Bottom,
//	Top,
//	Left,
//	Right,
//};

struct CFDCell {
	enum Type {
		HEXAHEDRON,
		TRIPRISM,
		TETRAHEDRON,
		PENTAHEDRON,
	} type;

	struct Face {
		cell_handle neighbor;
		vec3 normal;
	};

	Face faces[6];
	vertex_handle vertices[8];
};

struct CFDPolygon {
	enum Type {
		TRIANGLE = 3,
		QUAD = 4,
	} type;

	struct Edge {
		polygon_handle neighbor;
		uint neighbor_edge;
	};

	vertex_handle vertices[4];
	Edge edges[4];
    vec3 normal;
};

struct CFDVertex {
	vec3 position;
};

struct CFDSurface {
	vector<CFDPolygon> polygons;
	slice<CFDVertex> vertices;
};

struct CFDVolume {
	vector<CFDCell> cells;
	vector<CFDVertex> vertices;
    
    inline CFDCell& operator[](cell_handle cell) {
        return cells[cell.id];
    }
    
    inline CFDVertex& operator[](vertex_handle vertex) {
        return vertices[vertex.id];
    }
};

struct CFDMeshError {
	enum Type { None, NoMeshOrDomain, MeshOutsideDomain } type = None;
	uint id;
};

struct CFDDomain;
struct CFDMesh;
struct World;

struct ShapeDesc {
	struct Face {
		uint num_verts;
		uint verts[6];
        
        const uint operator[](uint v) const { return verts[v]; }
	};

	uint num_verts;
	uint num_faces;
	Face faces[6];
    
    const Face& operator[](uint face) const { return faces[face]; }
};

constexpr ShapeDesc hexahedron_shape = { 8, 6, {
	{4,{3,2,1,0}}, //bottom
	{4,{0,4,7,3}}, //left
	{4,{2,3,7,6}}, //back
	{4,{1,2,6,5}},//right
	{4,{0,1,5,4}},//front
	{4,{4,5,6,7}},//top
} };

constexpr ShapeDesc triprism_shape = { 6, 5, {
	{3, {0,1,2}}, //bottom
	{4, {0,3,5,2}}, //left
	{4, {1,2,5,4}}, //right
	{4, {0,1,4,3}}, //front
	{3, {3,4,5}},//top
} };

constexpr ShapeDesc tetra_shape = { 4, 4, {
	{3, {2,1,0, 3}}, //bottom
	{3, {0,3,2, 1}}, //left
	{3, {1,2,3, 0}}, //right
	{3, {0,1,3, 2}}, //front
} };

constexpr uint tetra_opposite[4] = { 3, 1, 0, 2 };

constexpr ShapeDesc petra_shape = { 5, 5, {
	{4, {0,1,2,3}}, //bottom
	{3, {3,4,0}}, //left
	{3, {1,0,4}}, //back
	{3, {2,1,4}}, //right
	{3, {3,2,4}}, //front
} };

constexpr ShapeDesc shapes[4] = {
	hexahedron_shape, 
	triprism_shape,
	tetra_shape,
	petra_shape
};

void compute_normals(slice<CFDVertex> vertices, CFDCell& cell);

vec3 triangle_normal(vec3 positions[3]);
vec3 triangle_center(vec3 positions[3]);
vec3 quad_normal(vec3 positions[4]);
void advancing_front_triangulation(CFDVolume& mesh, uint& extruded_vertices_offset, uint& extruded_cell_offset, const AABB&);
//void build_delaunay(CFDVolume& volume, slice<vertex_handle> verts, slice<Boundary> boundary);
CFDVolume generate_mesh(World& world, struct InputMeshRegistry&, CFDMeshError&, struct CFDDebugRenderer&);
void log_error(CFDMeshError& error);

inline void get_positions(slice<CFDVertex> vertices, const CFDPolygon& polygon, vec3* positions) {
	uint verts = polygon.type;
	for (uint i = 0; i < verts; i++) {
		positions[i] = vertices[polygon.vertices[i].id].position;
	}
}

inline void get_positions(slice<CFDVertex> vertices, slice<vertex_handle> handles, vec3* positions) {
	for (uint i = 0; i < handles.length; i++) {
		positions[i] = vertices[handles[i].id].position;
	}
}

inline void get_positions(slice<CFDVertex> vertices, CFDCell& cell, const ShapeDesc::Face& face, vec3* positions) {
	uint verts = face.num_verts;
	for (uint j = 0; j < face.num_verts; j++) {
		vertex_handle vertex = cell.vertices[face.verts[j]];
		positions[j] = vertices[vertex.id].position;
	}
}

inline vec3 compute_centroid(CFDVolume& volume, vertex_handle vertices[], uint n) {
    vec3 centroid;
    for (uint i = 0; i < n; i++) {
        centroid += volume[vertices[i]].position;
    }
    centroid /= n;
    return centroid;
}
