#pragma once

#include "core/core.h"
#include "core/math/aabb.h"
#include "core/container/vector.h"

struct SurfaceTriMesh;
struct Ray;
struct CFDDebugRenderer;

enum class MeshPrimitive;

//does it make sense to seperately store triangles, edges, vertices
//when both can be derived from the triangle
struct InputModelBVH {
	SurfaceTriMesh& model;
	CFDDebugRenderer& debug;

	using tri_handle = uint;

	struct RayHit {
		uint id;
		float t = FLT_MAX;
	};

	struct Node {
		AABB aabb;

		uint triangle_count;
		uint vertex_count;
		uint triangle_offset;
		uint vertex_offset;

		uint children[2];
	};

	vector<tri_handle> triangles;
	vector<uint> vertices;
	vector<Node> nodes;

	InputModelBVH(SurfaceTriMesh&, CFDDebugRenderer&);
	uint build_node(const AABB& aabb, slice<uint> triangles, slice<uint> vertices, uint depth);
	void build();

	bool ray_intersect(const Ray& ray, const glm::mat4& model, RayHit& hit, MeshPrimitive primitive);
	void ray_intersect(Node& node, Ray& ray, RayHit& hit, MeshPrimitive primitive);
};