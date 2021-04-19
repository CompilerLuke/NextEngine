//NOTE: Replaced by Constrained Deluanay Triangulation and Refinement
//Naive algorithm generated intersecting elements and of bad quality,
//going from a cube face to an isotropic layer, created 8 tetrahedra
//which means the elements got smaller instead of larger


#include "core/math/aabb.h"
#include "mesh.h"
#include "core/memory/linear_allocator.h"
#include "mesh_generation/front_octotree.h"
#include "core/math/intersection.h"

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

	Front::Result result = front.find_closest(glm::vec3(0, 1.0, 0.0), vec3(0), 0.8, { 0 });
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

	Front::Result result = front.find_closest(position, centroid, r, curr);
	vertex_handle vertex = result.vertex;

	//Add cell to the front and mesh
	int cell_id = mesh.cells.length;

	if (vertex.id == -1) {
		uint attempts = 5;
		for (uint i = 0; i < attempts; i++) {
			vec3 position = centroid + glm::normalize(normal) * r * (attempts - i)/attempts * r_h;

			if (i+1 < attempts && front.intersects(Ray{ centroid,position })) continue;
			
			vertex.id = mesh.vertices.length;
			cell.vertices[base_sides] = vertex;
			mesh.vertices.append({ position });
		}
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
