#pragma once

#include "core/core.h"
#include "core/container/vector.h"
#include "core/container/array.h"
#include "core/math/vec3.h"
#include "graphics/culling/aabb.h"

struct vertex_handle {
	int id = -1;
};

struct polygon_handle {
	int id = -1;
};

struct cell_handle {
	int id = -1;
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
	float pressure;
	vec3 velocity;
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
};

struct CFDVertex {
	vec3 position;
	vec3 normal;
};

struct CFDSurface {
	vector<CFDPolygon> polygons;
	slice<CFDVertex> vertices;
};

struct CFDVolume {
	vector<CFDCell> cells;
	vector<CFDVertex> vertices;
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
	};

	uint num_verts;
	uint num_faces;
	Face faces[6];
};

constexpr ShapeDesc hexahedron_shape = { 8, 6, {
	{4,{0,1,2,3}}, //bottom
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
	{3, {0,1,2}}, //bottom
	{3, {0,3,2}}, //left
	{3, {1,2,3}}, //right
	{3, {0,1,3}}, //front
} };

constexpr ShapeDesc petra_shape = { 5, 5, {
	{4, {0,1,2,3}}, //bottom
	{3, {0,4,3}}, //left
	{3, {2,3,4}}, //back
	{3, {1,2,4}}, //right
	{3, {0,1,4}}, //front
} };

constexpr ShapeDesc shapes[4] = {
	hexahedron_shape, 
	triprism_shape,
	tetra_shape,
	petra_shape
};

CFDVolume generate_mesh(World& world, CFDMeshError&);
void log_error(CFDMeshError& error);