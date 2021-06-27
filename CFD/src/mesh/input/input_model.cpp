#include "mesh/input_mesh.h"
#include "core/math/intersection.h"
#include "graphics/culling/scene_partition.h"
#include "mesh/input_mesh_bvh.h"
#include "visualization/debug_renderer.h"
#include "editor/selection.h"

InputModelBVH::InputModelBVH(SurfaceTriMesh& model, CFDDebugRenderer& debug) : model(model), debug(debug) {

}

uint InputModelBVH::build_node(const AABB& aabb, slice<tri_handle> triangles, slice<uint> vertices, uint depth) {
	const uint MAX_DEPTH = 20;
	const uint MAX_PER_NODE = 4;

	if ((triangles.length < MAX_PER_NODE && vertices.length < MAX_PER_NODE) || depth == MAX_DEPTH) {
		Node node = {};
		node.aabb = aabb;
		node.triangle_count = triangles.length;
		node.vertex_count = vertices.length;

		node.triangle_offset = this->triangles.length;
		node.vertex_offset = this->vertices.length;

		this->triangles += triangles;
		this->vertices += vertices;
		
		nodes.append(node);
		return nodes.length - 1;
	}
	else {
		LinearAllocator& temporary = get_thread_local_temporary_allocator();

		glm::vec3 size = aabb.size();
		uint axis = size.x > size.y ? (size.x > size.z ? 0 : 2) : (size.y > size.z ? 1 : 2);
		glm::vec3 pivot = aabb.centroid();

		AABB subdivided_aabb[2];
		tvector<tri_handle> subdivided_triangles[2];
		tvector<uint> subdivided_vertices[2];

		for (tri_handle triangle : triangles) {
			glm::vec3 p[3];
			for (uint i = 0; i < 3; i++) p[i] = model.position(triangle, i);
					
			glm::vec3 center = (p[0] + p[1] + p[2]) / 3.0f;
			
			uint node = center[axis] > pivot[axis];
			subdivided_triangles[node].append(triangle);
			
			for (uint i = 0; i < 3; i++) subdivided_aabb[node].update(p[i]);
		}

		for (uint vertex : vertices) {
			glm::vec3 p = model.positions[vertex];
			uint node = p[axis] > pivot[axis];
			subdivided_vertices[node].append(vertex);
			subdivided_aabb[node].update(p);
		}


		uint offset = nodes.length;
		nodes.append({ aabb });
		for (uint i = 0; i < 2; i++) {
			nodes[offset].children[i] = build_node(subdivided_aabb[i], subdivided_triangles[i], subdivided_vertices[i], depth+1);
		}
		return offset;
	}
}

void InputModelBVH::build() {
	tvector<tri_handle> triangles;
	triangles.reserve(model.tri_count);
	
	slice<vertex_handle> indices = { model.indices, 3 * model.tri_count };
	
	for (tri_handle triangle : model) {
		triangles.append(triangle);
	}

	build_node(model.aabb, triangles, vertices, 0);
}


bool InputModelBVH::ray_intersect(const Ray& world_ray, const glm::mat4& to_world, RayHit& hit, MeshPrimitive primitive) {
	if (nodes.length == 0) return false;
	
	glm::mat4 to_model = glm::inverse(to_world);
	Ray mesh_ray{
		to_model * glm::vec4(world_ray.orig,1.0),
		to_model * glm::vec4(world_ray.dir,1.0),
		world_ray.t
	};

	//draw_line(debug, mesh_ray.orig, mesh_ray.dir * mesh_ray.t, vec4(1, 0, 0, 1));
	
	ray_intersect(nodes[0], mesh_ray, hit, primitive);

	bool intersection = hit.t < FLT_MAX;
	if (intersection && primitive == MeshPrimitive::Edge) {
		vec3 p2 = mesh_ray.orig + mesh_ray.dir * hit.t;

		int edge = -1;
		float smallest_dist = FLT_MAX;
		for (uint i = 0; i < 3; i++) {
			vertex_handle v0 = model.indices[hit.id + i];
			vertex_handle v1 = model.indices[hit.id + (i + 1) % 3];

			vec3 p0 = model.positions[v0.id];
			vec3 p1 = model.positions[v1.id];
			
			vec3 p01 = normalize(p1 - p0);
			vec3 p02 = p2 - p0;
			float t = dot(p01, p02);
			vec3 p = p0 + p01 * t;
			float dist = length(p - p2);

			if (dist < smallest_dist) {
				smallest_dist = dist;
				
				if (v0.id < v1.id) edge = hit.id + i;
				else edge = model.edges[hit.id + i];
			}
		}

		if (edge >= 0) hit.id = edge;
		else intersection = false;
	}

	return intersection;
}

#include "mesh.h"

void draw_cube(CFDDebugRenderer& debug, const AABB& aabb) {
	glm::vec3 verts[8];
	aabb.to_verts(verts);
	
	const ShapeDesc& shape = hexahedron_shape;
	for (uint i = 0; i < shape.num_faces; i++) {
		for (uint j = 0; j < 4; j++) {
			draw_line(debug, verts[shape[i][j]], verts[shape[i][(j + 1) % 4]], vec4(1,0,0,1));
		}
	}
}

void InputModelBVH::ray_intersect(Node& node, Ray& ray, RayHit& hit, MeshPrimitive primitive) {
	if (!ray_aabb_intersect(node.aabb, ray)) return;

	//draw_cube(debug, node.aabb);

	bool has_children = node.children[0];
	if (has_children) {
		for (uint i = 0; i < 2; i++) {
			ray_intersect(nodes[node.children[i]], ray, hit, primitive);
		}
	}
	else {
		for (uint i = node.triangle_offset; i < node.triangle_offset+node.triangle_count; i++) {
			tri_handle triangle = triangles[i];
			vec3 p[3];
			for (uint j = 0; j < 3; j++) p[j] = model.position(triangle, j);

			float t;
			if (primitive == MeshPrimitive::Triangle || primitive == MeshPrimitive::Edge) {
				if (ray_triangle_intersect(ray, p, &t)) {
					hit.t = t;
                    //hit.node = &node;
					ray.t = t; //next intersection must be closer
					hit.id = triangle; //todo not triangle id
				}
			}
		}
	}
}
