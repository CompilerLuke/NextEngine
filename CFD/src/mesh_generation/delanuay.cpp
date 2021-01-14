#include "core/math/vec4.h"
#include "core/math/vec3.h"
#include <glm/mat3x3.hpp>
#include "core/container/tvector.h"
#include "core/container/hash_map.h"
#define _USE_MATH_DEFINES
#include <math.h>

float sq_distance(glm::vec3 a, glm::vec3 b) {
	glm::vec3 dist3 = a - b;
	return glm::dot(dist3, dist3);
}

#define MATRIX_CIRCUM_IMPL

vec4 circumsphere(vec3 p[4]) {
#ifndef MATRIX_CIRCUM_IMPL
	vec3 u1 = p[0] - p[3];
	vec3 u2 = p[1] - p[3];
	vec3 u3 = p[2] - p[3];

	vec3 center = (sq_distance(p[0], p[3]) * cross(u2, u3)
		+ sq_distance(p[1], p[3]) * cross(u3, u1)
		+ sq_distance(p[2], p[3]) * cross(u1, u2))
		/ (2 * dot(u1, cross(u2, u3)));
	center += p[3];
	float r = length(center - p[0]);
	return vec4(center, r);


#else

	glm::mat3 a = glm::transpose(glm::mat3(p[0] - p[3], p[1] - p[3], p[2] - p[3]));
	float sp = sq(p[3]);
	vec3 b = 0.5f * vec3(sq(p[0]) - sp, sq(p[1]) - sp, sq(p[2]) - sp);
	vec3 c = glm::inverse(a) * b;

	float det = (
		+a[0][0] * (a[1][1] * a[2][2] - a[2][1] * a[1][2])
		- a[1][0] * (a[0][1] * a[2][2] - a[2][1] * a[0][2])
		+ a[2][0] * (a[0][1] * a[1][2] - a[1][1] * a[0][2]));

	if (fabs(det) < 0.001) {
		//printf("Co-planar points\n");
		c = (p[0] + p[1] + p[2] + p[3]) / 4;
	//return vec4((p[0]+p[1]+p[2]+p[3])/3, length(p[0]-p[1]));
	}

	float r = length(c - p[0]);

	//assert(det != 0.0f);
	//assert(r < 1000);
	//assert(!isnan(c.x) && !isnan(c.y) && !isnan(c.z));

	return vec4(c,r);
#endif
}

#include "graphics/culling/aabb.h"
#include "mesh.h"
#include "core/memory/linear_allocator.h"

struct DeluanayAcceleration {
	static constexpr uint MAX_PER_CELL = 16;
	static constexpr uint BLOCK_SIZE = kb(8);

	union Payload;

	struct Subdivision {
		AABB aabb; //adjusts to content
		AABB aabb_division; //always exactly half of parent
		uint count;
		Payload* p; //to avoid recursion
		Subdivision* parent;
	};

	struct TetElement {
		cell_handle cell;
		vec3 circumcenter;
		float circumradius;

		AABB aabb() const {
			return {circumcenter - circumradius, circumcenter + circumradius};
		}
	};

	union Payload {
		Subdivision children[8];
		TetElement elements[MAX_PER_CELL];
		Payload* free_next;

		Payload() {
			memset(this, 0, sizeof(Payload));
		}
	};

	struct CellInfo {
		Subdivision* subdivision = nullptr;
		uint index = 0;
	};

	struct Result {
		uint cell_count;
		cell_handle cells[16];
		vertex_handle vertex;
		float dist = FLT_MAX;
		float dist_to_center = FLT_MAX;
	};

	Payload* free_payload;
	Subdivision root;
	AABB grid_bounds;
	vector<CFDVertex>& vertices;
	vector<CFDCell>& cells;
	vector<CellInfo> cell_info;

	DeluanayAcceleration(vector<CFDVertex>& vertices, vector<CFDCell>& cells, const AABB& aabb)
		: vertices(vertices), cells(cells) {
		root = {};
		free_payload = nullptr;
		init(root);
		root.aabb_division = aabb;
		root.parent = nullptr;
	}

	bool is_leaf(Subdivision& subdivision) {
		return subdivision.count <= MAX_PER_CELL;
	}

	void init(Subdivision& subdivision) {
		if (!free_payload) alloc_new_block();

		subdivision = {};
		subdivision.p = free_payload;

		free_payload = free_payload->free_next;
	}

	//note: won't set cell_info of children to null
	void deinit(Subdivision& subdivision) {
		bool leaf = is_leaf(subdivision);

		if (!leaf) {
			for (uint i = 0; i < 8; i++) deinit(subdivision.p->children[i]);
		}

		subdivision.p->free_next = free_payload;
		free_payload = subdivision.p;
	}

	void alloc_new_block() {
		//todo cleanup alloced blocks
		uint count = BLOCK_SIZE / sizeof(Payload);
		Payload* payloads = TEMPORARY_ARRAY(Payload, count);

		for (uint i = 1; i < count; i++) {
			payloads[i-1].free_next = payloads + i;
		}
		payloads[count-1].free_next = nullptr;

		free_payload = payloads;
	}

	void find_in_circum(tvector<cell_handle>& cells, Subdivision& subdivision, vec3 point) {
		bool leaf = is_leaf(subdivision);
		if (leaf) {
			for (uint i = 0; i < subdivision.count; i++) {
				TetElement& e = subdivision.p->elements[i];
				if (sq_distance(e.circumcenter, point) + FLT_EPSILON < e.circumradius * e.circumradius) {
					cells.append(e.cell);
				}
			}
		}
		else {
			for (uint i = 0; i < 8; i++) {
				if (!subdivision.p->children[i].aabb.inside(point)) continue;
				find_in_circum(cells, subdivision.p->children[i], point);
			}
		}
	}

	void find_in_circum(tvector<cell_handle>& cells, vec3 pos) {
		find_in_circum(cells, root, pos);
	}

	uint centroid_to_index(uint tiebreaker, vec3 centroid, vec3 min, vec3 half_size) {
		vec3 vec = (centroid - min + FLT_EPSILON) / (half_size + 2*FLT_EPSILON);
		//assert(vec.x >= -0.1f && vec.x <= 2.1f);
		//assert(vec.y >= -0.1f && vec.y <= 2.1f);
		//assert(vec.z >= -0.1f && vec.z <= 2.1f);
		int x = (int)vec.x;
		int y = (int)vec.y;
		int z = (int)vec.z;

		if (x < 0 || x > 1 || y < 0 || y > 1 || z < 0 || z > 1) {
			return tiebreaker % 8;
		}
		//x = (x > 1) ? 1 : (x < 0) ? 0 : x;
		//y = (y > 1) ? 1 : (y < 0) ? 0 : y;
		//z = (z > 1) ? 1 : (z < 0) ? 0 : z;

		return x+ 4*y + 2*z;
	}

	void add_cell_to_leaf(Subdivision& subdivision, const TetElement& e) {
		uint index = subdivision.count++;
		subdivision.aabb.update_aabb(e.aabb());
		subdivision.p->elements[index] = e;
		cell_info[e.cell.id].index = index;
		cell_info[e.cell.id].subdivision = &subdivision;
		//printf("Set cell info %i to %p\n", e.cell.id, &subdivision);
	}

	void add_cell_to_sub(Subdivision divisions[8], const  TetElement e, glm::vec3 min, glm::vec3 half_size) {
		uint index = centroid_to_index(e.cell.id, e.circumcenter, min, half_size);
		add_cell_to_leaf(divisions[index], e);
	}

	void add_cell(cell_handle cell, vec4 circum) {
		if (cell.id >= cell_info.length) {
			cell_info.resize(cell.id + 1);
		}

		TetElement e;
		e.cell = cell;
		e.circumcenter = vec3(circum);
		e.circumradius = circum.w;
		
		//assert(root.aabb_division.inside(vec3(circum)));
		//printf("Circumcenter[%i] %f %f %f, radius %f\n", cell.id, circum.x, circum.y, circum.z, circum.w);

		//printf("==========\n");

		Subdivision* subdivision = &root;

		uint depth = 0;
		while (true) {
			bool leaf = is_leaf(*subdivision);

			vec3 half_size = 0.5f * subdivision->aabb_division.size();
			vec3 min = subdivision->aabb_division.min;

			if (leaf) {
				if (subdivision->count < MAX_PER_CELL) {
					add_cell_to_leaf(*subdivision, e);
					//printf("Adding node %i\n", depth);
					break;
				}
				else { //Subdivide the cell
					TetElement elements[MAX_PER_CELL];
					memcpy_t(elements, subdivision->p->elements, subdivision->count);

					bool all_duplicates = true;
					bool all_outside = true;

					vec3 last;
					for (uint i = 0; i < MAX_PER_CELL && (all_duplicates || all_outside); i++) {
						vec3 current = elements[i].circumcenter;
						float dist = fabs(sq_distance(last, current));
						if (i > 0 && all_duplicates && dist > 0.01) {
							all_duplicates = false;
						}
						if (all_outside && subdivision->aabb_division.inside(current)) {
							all_outside = false;
						}
						last = current;
					}

					bool unsplittable = all_duplicates || all_outside;

					//assert(depth < 20);

					//printf("Subdividing %p (%i)\n", subdivision, subdivision->count);

					glm::vec3 mins[8] = {
						//bottom
						{glm::vec3(min.x,               min.y,               min.z)}, //left front
						{glm::vec3(min.x + half_size.x, min.y,               min.z)}, //right front
						{glm::vec3(min.x,               min.y,               min.z + half_size.z)}, //left back
						{glm::vec3(min.x + half_size.x, min.y,               min.z + half_size.z)}, //right back
						//top
						{glm::vec3(min.x,               min.y + half_size.y, min.z)}, //left front
						{glm::vec3(min.x + half_size.x, min.y + half_size.y, min.z)}, //right front
						{glm::vec3(min.x,               min.y + half_size.y, min.z + half_size.z)}, //left back
						{glm::vec3(min.x + half_size.x, min.y + half_size.y, min.z + half_size.z)}, //right back
					};

					for (uint i = 0; i < 8; i++) {
						init(subdivision->p->children[i]);
						//printf("Initited %p\n", subdivision->p->children + i);
						subdivision->p->children[i].aabb_division = { mins[i], mins[i] + half_size };
						subdivision->p->children[i].parent = subdivision;
					}


					if (unsplittable) {
						for (uint i = 0; i < MAX_PER_CELL; i++) {
							add_cell_to_leaf(subdivision->p->children[i%8], elements[i]);
						}
					}
					else {
						for (uint i = 0; i < MAX_PER_CELL; i++) {
							add_cell_to_sub(subdivision->p->children, elements[i], min, half_size);
						}
					}

					//printf("Has count (%i)\n", subdivision->count);

				}
			}

			uint child_count = 0;
			for (uint i = 0; i < 8; i++) {
				child_count += subdivision->p->children[i].count;
				assert(subdivision->p->children[i].p);
			}
			assert(child_count == subdivision->count);

			//printf("Visited node %p %i\n", subdivision, subdivision->count);
	
			uint index = centroid_to_index(e.cell.id, e.circumcenter, min, half_size);
			subdivision->count++;
			subdivision->aabb.update_aabb({e.circumcenter - e.circumradius, e.circumcenter + e.circumradius});
			
			Subdivision* prev = subdivision;
			
			subdivision = subdivision->p->children + index;
			assert(subdivision != prev);
			assert(subdivision->p);

			depth++;
		}
	}

	void remove_cell(cell_handle handle) {
		CellInfo info = cell_info[handle.id];
		Subdivision* subdivision = info.subdivision;

		assert(subdivision->count > 0);
		uint index = info.index;
		uint last = subdivision->count - 1;

		AABB aabb = subdivision->p->elements[index].aabb();
		subdivision->p->elements[index] = subdivision->p->elements[last];
		cell_info[subdivision->p->elements[index].cell.id].index = index;
		cell_info[handle.id] = {};

		bool update_aabb = true;
		bool started_from = true;

		//printf("Removing node %p %i\n", subdivision, subdivision->count);

		//printf("Removed %i\n", handle.id);

		assert(subdivision);
		while (subdivision) {
			assert(subdivision->count > 0);
			subdivision->count--;
			assert(subdivision->p);

			const AABB& sub_aabb = subdivision->aabb;
			if (update_aabb) {
				update_aabb = glm::any(glm::equal(sub_aabb.min, aabb.min))
					|| glm::any(glm::equal(sub_aabb.max, aabb.max));
			}
			if (update_aabb) {
				subdivision->aabb = AABB();
				if (started_from) {
					for (uint i = 0; i < MAX_PER_CELL; i++) {
						subdivision->aabb.update_aabb(subdivision->p->elements[i].aabb());
					}
				}
				else {
					for (uint i = 0; i < 8; i++) {
						subdivision->aabb.update_aabb(subdivision->p->children[i].aabb);
					}
				}
			}

			if (is_leaf(*subdivision) && !started_from) {
				//printf("Unsubdividing %p\n", subdivision);
				Subdivision children[8];
				memcpy_t(children, subdivision->p->children, 8);

				subdivision->count = 0;
				for (uint i = 0; i < 8; i++) {
					Subdivision& child = children[i];

					for (uint j = 0; j < child.count; j++) {
						subdivision->p->elements[subdivision->count] = child.p->elements[j];

						cell_handle cell = child.p->elements[j].cell;
						cell_info[cell.id].index = subdivision->count;
						cell_info[cell.id].subdivision = subdivision;
						subdivision->count++;
					}

					deinit(child);
				}

				assert(subdivision->count <= MAX_PER_CELL);
			}

			subdivision = subdivision->parent;
			started_from = false;
			assert(!subdivision || subdivision->p);
		}
	}
};

struct PolygonCavityFace {
	uint count = 0;
	vertex_handle backfacing[3];
};

float tetrahedron_skewness(vec3 positions[4]) {
	float e1 = length(positions[0] - positions[1]);
	float e2 = length(positions[0] - positions[1]);
	float e3 = length(positions[1] - positions[2]);
	float e4 = length(positions[2] - positions[0]);
	float e5 = length(positions[0] - positions[3]);
	float e6 = length(positions[1] - positions[3]);

	float al = (e1 + e2 + e3 + e4 + e5 + e6) / 6;
	float skewness = fabs(e1 - al) + fabs(e2 - al) + fabs(e3 - al) + fabs(e4 - al) + fabs(e5 - al) + fabs(e6 - al);
	skewness /= 6 * al;

	return skewness;
}

bool add_vertex(CFDVolume& volume, DeluanayAcceleration& acceleration, hash_map_base<TriangleFaceSet, PolygonCavityFace>& shared_face, uint shared_face_capacity, tvector<cell_handle>& in_circum, vertex_handle vert, uint depth = 0) {
	vec3 position = volume.vertices[vert.id].position;

	in_circum.clear();
	acceleration.find_in_circum(in_circum, position);

	const uint max_shared_face = in_circum.length * 6;
	assert(max_shared_face < shared_face_capacity);
	
	shared_face.capacity = max_shared_face;
	shared_face.clear();

	printf("Found %i in circum\n", in_circum.length);

	for (cell_handle cell_handle : in_circum) {
		//remove cell
		CFDCell& cell = volume.cells[cell_handle.id];
		for (uint i = 0; i < 4; i++) {
			vertex_handle verts[3];
			for (uint j = 0; j < 3; j++) {
				verts[j] = cell.vertices[tetra_shape.faces[i].verts[2 - j]];
			}

			PolygonCavityFace& face = shared_face[verts];
			if (face.count++ == 0) memcpy_t(face.backfacing, verts, 3);
		}
	}

	uint offset = 0;
	uint created = 0;
	for (uint i = 0; i < max_shared_face; i++) {
		PolygonCavityFace& face = shared_face.values[i];
		//Face is on the edge of the polygonal cavity left behind
		if (face.count != 1) continue;

		cell_handle free_cell_handle;
		if (offset < in_circum.length) {
			free_cell_handle = in_circum[offset++];
			acceleration.remove_cell(free_cell_handle);
		}
		else {
			free_cell_handle = { (int)volume.cells.length };
			volume.cells.append({ CFDCell::TETRAHEDRON });
		}
		CFDCell& free_cell = volume.cells[free_cell_handle.id];

		memcpy_t(free_cell.vertices, face.backfacing, 3);
		free_cell.vertices[3] = vert;

		vec3 positions[4];
		get_positions(volume.vertices, { free_cell.vertices, 4 }, positions);

		vec4 circum = circumsphere(positions);

		static vec4 last_circum;
		static vec3 last_positions[4];
		static vertex_handle last_vertices[4];

		if (last_circum == vec3(circum)) {
			circumsphere(positions);
		}
		last_circum = circum;
		memcpy_t(last_positions, positions, 4);
		memcpy_t(last_vertices, free_cell.vertices, 4);
		
		acceleration.add_cell(free_cell_handle, circum);
		created++;
	}

	assert(created >= 3);

	//printf("Re-triangulated cavity %i\n", created);
	//todo not sure this is necessary
	//assert(offset == in_circum.length);
	for (uint i = offset; i < in_circum.length; i++) {
		cell_handle cell = in_circum[i];
		//printf("Deleting but not reclaiming cell!");
		volume.cells[cell.id].vertices[0].id = 0;
		volume.cells[cell.id].vertices[1].id = 0;
		volume.cells[cell.id].vertices[2].id = 0;
		volume.cells[cell.id].vertices[3].id = 0;
	}

	return true;
}

void build_deluanay(CFDVolume& volume, slice<vertex_handle> verts, slice<Boundary> boundary) {
	//generate super triangle
	vec3 centroid;
	for (vertex_handle vertex : verts) {
		centroid += volume.vertices[vertex.id].position / (float)verts.length;
	}

	float r = 0.0f;
	for (vertex_handle vertex : verts) {
		//doesn't work for distances smaller than 1.0
		r = fmaxf(r, length(volume.vertices[vertex.id].position - centroid));
	}

	float l = sqrtf(24)*r; //Super equilateral tetrahedron side length
	float b = -r;
	float t = sqrtf(6)/3*l - r; 
	float e = sqrtf(3)/3*l;
	float a = M_PI * 2 / 3;

	int super_vert_offset = volume.vertices.length;
	int super_cell_offset = volume.cells.length;

	volume.vertices.append({ centroid + vec3(e * cosf(0 * a), b, e * sinf(0 * a)) });
	volume.vertices.append({ centroid + vec3(e * cosf(1 * a), b, e * sinf(1 * a)) });
	volume.vertices.append({ centroid + vec3(e * cosf(2 * a), b, e * sinf(2 * a)) });
	volume.vertices.append({ centroid + vec3(0, t, 0) });

	

	CFDCell cell{CFDCell::TETRAHEDRON};
	vec3 positions[4];

	for (int i = 0; i < 4; i++) {
		cell.vertices[i] = { super_vert_offset + i };
		positions[i] = volume.vertices[super_vert_offset + i].position;
	}

	vec4 circum = circumsphere(positions);
	float m = 10.0f;
	AABB super_aabb = { vec3(circum) - m*circum.w, vec3(circum) + m*circum.w };

	int cell_id = volume.cells.length;
	volume.cells.append(cell);

	//Boyer-Watson, with octohedron acceleration structure
	DeluanayAcceleration acceleration(volume.vertices, volume.cells, super_aabb);
	acceleration.add_cell({ cell_id }, circum);

	LinearAllocator& allocator = get_temporary_allocator();

	tvector<cell_handle> in_circum;
	in_circum.allocator = &allocator;

	uint max_shared_faces = verts.length; // mb(1) / (sizeof(TriangleFaceSet) + sizeof(PolygonCavityFace) + sizeof(hash_meta));
	hash_map_base<TriangleFaceSet, PolygonCavityFace> shared_face = {
		max_shared_faces,
		alloc_t<hash_meta>(allocator, max_shared_faces),
		alloc_t<TriangleFaceSet>(allocator, max_shared_faces),
		alloc_t<PolygonCavityFace>(allocator, max_shared_faces)
	};

	cell_handle free_list;

	for (uint i = 0; i < verts.length; i++) {
		printf("Adding vertex %i of %i", i, verts.length);
		if (!add_vertex(volume, acceleration, shared_face, max_shared_faces, in_circum, verts[i])) {
			break;
		}
	}

	//Cleanup
	for (uint i = super_cell_offset; i < volume.cells.length; i++) {
		CFDCell& cell = volume.cells[i];
		bool destroy = false;
		for (uint j = 0; j < 4; j++) {
			uint index = cell.vertices[j].id;
			if (index >= super_vert_offset && index < super_vert_offset + 4) {
				destroy = true;
				break;
			}
		}

		if (destroy) {
			for (uint j = 0; j < 4; j++) cell.vertices[j].id = 0;
		}
	}

	//refine
	/*
	uint n = 3;
	for (uint i = 0; i < n; i++) {
		for (uint i = super_cell_offset; i < volume.cells.length; i++) {
			CFDCell& cell = volume.cells[i];
			vec3 positions[4];

			get_positions(volume.vertices, { cell.vertices,4 }, positions);
			float skewness = tetrahedron_skewness(positions);

			if (skewness > 0.5) {
				vec3 circum_center = circumsphere(positions);

				vertex_handle v = { (int)volume.vertices.length };
				volume.vertices.append({ circum_center });
				add_vertex(volume, acceleration, shared_face, in_circum, v);
			}
		}
	}
	*/

}