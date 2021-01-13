//NOTE: Replaced by Constrained Deluanay Triangulation and Refinement
//Naive algorithm generated intersecting elements and of bad quality,
//going from a cube face to an isotropic layer, created 8 tetrahedra
//which means the elements got smaller instead of larger


#include "graphics/culling/aabb.h"
#include "mesh.h"
#include "core/memory/linear_allocator.h"

struct Front {
	static constexpr uint MAX_PER_CELL = 16;
	static constexpr uint BLOCK_SIZE = kb(16);

	union Payload;

	struct Subdivision {
		AABB aabb; //adjusts to content
		AABB aabb_division; //always exactly half of parent
		uint count;
		Payload* p; //to avoid recursion
		Subdivision* parent;
	};



	union Payload {
		Subdivision children[8];
		struct {
			AABB aabbs[MAX_PER_CELL];
			cell_handle cells[MAX_PER_CELL];
		};
		Payload* free_next;

		Payload() {
			memset(this, 0, sizeof(Payload));
		}
	};

	struct CellInfo {
		Subdivision* subdivision;
		uint index;
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

	Subdivision* last_visited;

	Front(vector<CFDVertex>& vertices, vector<CFDCell>& cells, const AABB& aabb)
		: vertices(vertices), cells(cells) {
		root = {};
		free_payload = nullptr;
		init(root);
		root.aabb_division = aabb;
		last_visited = &root; //note: self-referential so moving this struct will cause problems!
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

	void deinit(Subdivision& subdivision) {
		bool leaf = is_leaf(subdivision);

		if (leaf) {
			for (uint i = 0; i < subdivision.count; i++) {
				cell_handle cell = subdivision.p->cells[i];
				cell_info[cell.id] = {};
			}
		}
		else {
			for (uint i = 0; i < 8; i++) deinit(subdivision.p->children[i]);
		}

		subdivision.p->free_next = free_payload;
		free_payload = subdivision.p;
	}

	void alloc_new_block() {
		//todo cleanup alloced blocks
		uint count = BLOCK_SIZE / sizeof(Payload);
		Payload* payloads = TEMPORARY_ARRAY(Payload, count);

		Payload* free_next = free_payload;

		for (uint i = 0; i < count; i++) {
			payloads[i] = {};
			payloads[i].free_next = free_next;
			free_next = payloads + i;
		}

		free_payload = free_next;
	}

	Result find_closest(Subdivision& subdivision, AABB& aabb, cell_handle curr) {
		Result result;
		if (!aabb.intersects(subdivision.aabb)) return result;

		glm::vec3 center = aabb.centroid();

		bool leaf = is_leaf(subdivision);
		if (leaf) {
			last_visited = &subdivision;

			for (uint i = 0; i < subdivision.count; i++) {
				if (subdivision.p->cells[i].id == curr.id) continue;
				if (!subdivision.p->aabbs[i].intersects(aabb)) continue;

				cell_handle cell_handle = subdivision.p->cells[i];
				CFDCell& cell = cells[cell_handle.id];


				//todo could skip this, if center is further than some threschold
				const ShapeDesc& shape = shapes[cell.type];
				bool valid_verts[8] = {};
				for (uint i = 0; i < shape.num_faces; i++) {
					const ShapeDesc::Face& face = shape.faces[i];
					if (cell.faces[i].neighbor.id != -1) continue;
					for (uint j = 0; j < face.num_verts; j++) {
						valid_verts[face.verts[j]] = true;
					}
				}

				uint verts = shapes[cell.type].num_verts;
				for (uint i = 0; i < verts; i++) {
					if (!valid_verts[i]) continue;

					vertex_handle v = cell.vertices[i];
					vec3 position = vertices[v.id].position;
					float dist = length(position - center);

					if (result.vertex.id == v.id) {
						result.cells[result.cell_count++] = cell_handle;
					}
					else if (dist < result.dist) {
						result.dist = dist;
						result.cell_count = 1;
						result.cells[0] = cell_handle;
						result.vertex = v;
					}
				}
			}
		}
		else {
			for (uint i = 0; i < 8; i++) {
				Result child_result = find_closest(subdivision.p->children[i], aabb, curr);
				if (child_result.dist < result.dist) result = child_result;
			}
		}

		return result;
	}

	Result find_closest(glm::vec3 position, float radius, cell_handle curr) {
		uint depth = 0;

		AABB sphere_aabb;
		sphere_aabb.min = position - glm::vec3(radius);
		sphere_aabb.max = position + glm::vec3(radius);

		Subdivision* start_from = last_visited;

		while (!sphere_aabb.inside(start_from->aabb_division)) {
			if (!start_from->parent) break; //found root
			start_from = start_from->parent;
		}

		Result result = find_closest(*start_from, sphere_aabb, curr);
		if (result.dist < radius) {
			return result;
		}
		else {
			return {};
		}
	}

	uint centroid_to_index(glm::vec3 centroid, glm::vec3 min, glm::vec3 half_size) {
		glm::vec3 vec = (centroid - min + FLT_EPSILON) / (half_size + 2 * FLT_EPSILON);
		assert(vec.x >= 0.0f && vec.x <= 2.0f);
		assert(vec.y >= 0.0f && vec.y <= 2.0f);
		return (int)floorf(vec.x) + 4 * (int)floorf(vec.y) + 2 * (int)floorf(vec.z);
	}

	void add_cell_to_leaf(Subdivision& subdivision, cell_handle handle, const AABB& aabb) {
		uint index = subdivision.count++;
		subdivision.aabb.update_aabb(aabb);
		subdivision.p->aabbs[index] = aabb;
		subdivision.p->cells[index] = handle;

		cell_info[handle.id].index = index;
		cell_info[handle.id].subdivision = &subdivision;
	}

	void add_cell_to_sub(Subdivision divisions[8], cell_handle cell, const AABB& aabb, glm::vec3 min, glm::vec3 half_size) {
		glm::vec3 centroid = aabb.centroid();
		uint index = centroid_to_index(centroid, min, half_size);

		add_cell_to_leaf(divisions[index], cell, aabb);
	}

	void add_cell(Subdivision& subdivision, cell_handle handle, glm::vec3 centroid, const AABB& aabb) {
		bool leaf = is_leaf(subdivision);

		glm::vec3 half_size = 0.5f * subdivision.aabb_division.size();
		glm::vec3 min = subdivision.aabb_division.min;

		if (leaf) {
			if (subdivision.count < MAX_PER_CELL) {
				last_visited = &subdivision;
				add_cell_to_leaf(subdivision, handle, aabb);
			}
			else { //Subdivide the cell
				Subdivision divisions[8];

				glm::vec3 mins[8] = {
					//bottom
					{glm::vec3(min.x,             min.y, min.z)}, //left front
					{glm::vec3(min.x + half_size.x, min.y, min.z)}, //right front
					{glm::vec3(min.x,             min.y, min.z + half_size.z)}, //left back
					{glm::vec3(min.x + half_size.x, min.y, min.z + half_size.z)}, //right back
					//top
					{glm::vec3(min.x,             min.y + half_size.y, min.z)}, //left front
					{glm::vec3(min.x + half_size.x, min.y + half_size.y, min.z)}, //right front
					{glm::vec3(min.x,             min.y + half_size.y, min.z + half_size.z)}, //left back
					{glm::vec3(min.x + half_size.x, min.y + half_size.y, min.z + half_size.z)}, //right back
				};

				for (uint i = 0; i < 8; i++) {
					init(divisions[i]);
					divisions[i].aabb_division = { mins[i], mins[i] + half_size };
					divisions[i].parent = &subdivision;
				}

				for (uint i = 0; i < MAX_PER_CELL; i++) {
					const AABB& aabb = subdivision.p->aabbs[i];
					cell_handle cell = subdivision.p->cells[i];

					add_cell_to_sub(divisions, cell, aabb, min, half_size);
				}

				memcpy_t(subdivision.p->children, divisions, 8);
				subdivision.count++;
				add_cell(subdivision, handle, centroid, aabb); //todo use loop instead of recursion
				subdivision.count--;
			}
		}
		else {
			uint index = centroid_to_index(centroid, min, half_size);

			add_cell(subdivision.p->children[index], handle, centroid, aabb);
			subdivision.aabb.update_aabb(aabb);
			subdivision.count++;
		}
	}

	void add_cell(cell_handle handle) {
		cell_info.resize(handle.id + 1);

		CFDCell& cell = cells[handle.id];

		AABB aabb = {};
		for (uint i = 0; i < shapes[cell.type].num_verts; i++) {
			vec3 position = vertices[cell.vertices[i].id].position;
			aabb.update(position);
		}

		//Subdivision* start_from = last_visited;
		//while (!aabb.inside(start_from->aabb_division)) {
		//	start_from = start_from->parent;
		//}

		add_cell(root, handle, aabb.centroid(), aabb);
	}

	void remove_cell(cell_handle handle) {
		CellInfo& info = cell_info[handle.id];
		Subdivision* subdivision = info.subdivision;

		uint index = info.index;
		uint last = subdivision->count - 1;

		AABB aabb = subdivision->p->aabbs[index];

		subdivision->p->aabbs[index] = subdivision->p->aabbs[last];
		subdivision->p->cells[index] = subdivision->p->cells[last];

		bool update_aabb = true;
		bool started_from = true;
		while (subdivision) {
			subdivision->count--;

			const AABB& sub_aabb = subdivision->aabb;
			if (update_aabb) {
				update_aabb = glm::any(glm::equal(sub_aabb.min, aabb.min))
					|| glm::any(glm::equal(sub_aabb.max, aabb.max));
			}
			if (update_aabb) {
				subdivision->aabb = AABB();
				if (started_from) {
					for (uint i = 0; i < MAX_PER_CELL; i++) {
						subdivision->aabb.update_aabb(subdivision->p->aabbs[i]);
					}
				}
				else {
					for (uint i = 0; i < 8; i++) {
						subdivision->aabb.update_aabb(subdivision->p->children[i].aabb);
					}
				}
			}

			if (is_leaf(*subdivision) && !started_from) {
				Subdivision children[8];
				memcpy_t(children, subdivision->p->children, 8);

				subdivision->count = 0;
				for (uint i = 0; i < 8; i++) {
					Subdivision& child = children[i];

					memcpy_t(subdivision->p->aabbs + subdivision->count, child.p->aabbs, child.count);
					memcpy_t(subdivision->p->cells + subdivision->count, child.p->cells, child.count);

					subdivision->count += child.count;
					deinit(child);
				}

				assert(subdivision->count <= MAX_PER_CELL);
			}

			subdivision = subdivision->parent;
			started_from = false;
		}
	}
};

void test_front() {
	vector<CFDVertex> vertices;
	vector<CFDCell> cells;

	AABB root_aabb{ glm::vec3(-10), glm::vec3(10) };
	Front front(vertices, cells, root_aabb);

	vertices.append({ glm::vec3(-0.5,0,-0.5) });
	vertices.append({ glm::vec3(-0.5,0,0.5) });
	vertices.append({ glm::vec3(0.5,0,0.5) });
	vertices.append({ glm::vec3(0.5,0,-0.5) });
	vertices.append({ glm::vec3(0.0,1.0,0.0) });

	CFDCell cell;
	cell.type = CFDCell::PENTAHEDRON;
	for (int i = 0; i < 5; i++) cell.vertices[i] = { i };
	cells.append(cell);

	front.add_cell({ 0 });

	Front::Result result = front.find_closest(glm::vec3(0, 1.0, 0.0), 0.8, { 0 });
	assert(result.cell_count == 1);
	assert(result.cells[0].id == 0);
	assert(result.vertex.id == 4);
}

bool verts4_match(const vertex_handle a[4], const vertex_handle b[4]) {
	vertex_handle a_copy[4];
	vertex_handle b_copy[4];

	memcpy_t(a_copy, a, 4);
	memcpy_t(b_copy, b, 4);

	for (uint i = 0; i < 4; i++) {
		if (a_copy[i].id != b_copy[i].id) return false;
	}

	return true;
}

void create_isotropic_cell(CFDVolume& mesh, Front& front, CFDCell& cell, cell_handle curr, vec3* positions, glm::vec3 normal) {
	const ShapeDesc& shape = shapes[cell.type];
	uint base_sides = shape.faces[0].num_verts;

	vec3 centroid;
	for (uint i = 0; i < base_sides; i++) centroid += positions[i];
	centroid /= base_sides;

	//length(positions[i] - positions[(i + 1) % base_sides])
	float r_h = base_sides == 3 ? sqrtf(6)/3 : sqrtf(2)/2;
	float r = FLT_MAX;
	for (uint i = 0; i < base_sides; i++) r = fminf(r, length(positions[i] - positions[(i+1)%base_sides]));
	//r /= base_sides;

	glm::vec3 position = centroid + glm::normalize(normal) * r * r_h;

	Front::Result result = front.find_closest(position, r, curr);
	vertex_handle vertex = result.vertex;

	//Add cell to the front and mesh
	int cell_id = mesh.cells.length;

	if (vertex.id == -1) {
		vertex.id = mesh.vertices.length;
		cell.vertices[base_sides] = vertex;
		mesh.vertices.append({ position, normal });
	}
	else {
		cell.vertices[base_sides] = vertex;

		for (uint i = 0; i < result.cell_count; i++) {
			cell_handle neighbor_cell = result.cells[i];
			CFDCell& neighbor = mesh.cells[neighbor_cell.id];

			vertex_handle faces_cell[6][3];
			vertex_handle faces_neighbor[6][3];

			for (uint i = 1; i < shape.num_faces; i++) {
				for (uint j = 0; j < 3; j++) {
					faces_cell[i][j] = cell.vertices[shape.faces[i].verts[j]];
				}
				sort3_vertices(faces_cell[i]);
			}

			const ShapeDesc& neighbor_shape = shapes[neighbor.type];

			for (uint i = 0; i < neighbor_shape.num_faces; i++) {
				for (uint j = 0; j < 3; j++) {
					faces_neighbor[i][j] = neighbor.vertices[neighbor_shape.faces[i].verts[j]];
				}
				sort3_vertices(faces_neighbor[i]);
			}

			for (uint i = 1; i < shape.num_faces; i++) {
				for (uint j = 0; j < neighbor_shape.num_faces; j++) {
					if (neighbor.faces[j].neighbor.id != -1) continue;
					if (memcmp(faces_cell + i, faces_neighbor + j, sizeof(vertex_handle) * 3) == 0) {
						cell.faces[i].neighbor = neighbor_cell;
						neighbor.faces[j].neighbor = { cell_id };
					}
				}
			}
		}
	}

	compute_normals(mesh.vertices, cell);
	mesh.cells.append(cell);
	front.add_cell({ cell_id });
}

void advancing_front_triangulation(CFDVolume& mesh, uint& extruded_vertices_offset, uint& extruded_cell_offset, const AABB& domain_bounds) {
	Front front(mesh.vertices, mesh.cells, domain_bounds);

	uint cell_count = mesh.cells.length;
	for (int cell_id = extruded_cell_offset; cell_id < cell_count; cell_id++) {
		front.add_cell({ cell_id });
	}

	uint unattached = 1;
	for (uint i = 0; i < 2; i++) {
		unattached = 0;

		uint cell_count = mesh.cells.length;
		for (uint cell_id = extruded_cell_offset; cell_id < cell_count; cell_id++) {


			const ShapeDesc& shape = shapes[mesh.cells[cell_id].type];

			//assume base is already connected
			for (uint i = 0; i < shape.num_faces; i++) {
				CFDCell& cell = mesh.cells[cell_id];
				if (cell.faces[i].neighbor.id == -1) {
					unattached++;
					const ShapeDesc::Face& face = shape.faces[i];

					vertex_handle vertices[4];
					vec3 positions[4];

					uint verts = face.num_verts;
					for (uint j = 0; j < face.num_verts; j++) {
						uint index = face.num_verts - j - 1; //flip vertices, as this face will be the bottom
						vertices[index] = cell.vertices[face.verts[j]];
						positions[index] = mesh.vertices[vertices[index].id].position;

					}

					CFDCell new_cell;
					vec3 normal;
					if (verts == 4) {
						new_cell.type = CFDCell::PENTAHEDRON;
						normal = -quad_normal(positions);
					}
					else {
						new_cell.type = CFDCell::TETRAHEDRON;
						normal = -triangle_normal(positions);
					}

					new_cell.faces[0].neighbor = { (int)cell_id };
					cell.faces[i].neighbor = { (int)mesh.cells.length };

					//todo set neighbor of base
					memcpy_t(new_cell.vertices, vertices, verts);
					create_isotropic_cell(mesh, front, new_cell, { (int)cell_id }, positions, normal);
				}
			}
		}
	}
}
