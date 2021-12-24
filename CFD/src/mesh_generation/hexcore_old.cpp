#include "mesh_generation/hexcore.h"
#include "core/container/tvector.h"
#include "mesh.h"
#include "mesh/surface_tet_mesh.h"

struct Grid {
	bool* occupied;
	bool* occupied2;
	vertex_handle* vertices;
	cell_handle* cell_ids;
	vector<vertex_handle>* boundary_verts;
	vector<Boundary>* boundary;
	uint width;
	uint height;
	uint depth;

	glm::ivec3 imin, imax;
	glm::vec3 min, max;
	float resolution;

	uint index(glm::ivec3 vec) {
		return vec.x + vec.y * width + vec.z * width * height;
	}

	glm::ivec3 pos(uint index) {
		int wh = width * height;
		glm::ivec3 pos;
		pos.z = index / wh;
		int rem = index - pos.z * wh;
		pos.y = rem / width;
		pos.x = rem % width;
		return pos;
	}

	bool is_valid(glm::ivec3 p) {
		return p.x >= 0 && p.x < width
			&& p.y >= 0 && p.y < height
			&& p.z >= 0 && p.z < depth;
	}

	bool& operator[](glm::ivec3 vec) {
		return occupied[index(vec)];
	}
};

glm::ivec3 grid_offsets[6] = {
	glm::ivec3(0,-1,0),
	glm::ivec3(-1,0 ,0),
	glm::ivec3(0 ,0 ,-1),
	glm::ivec3(1 ,0 ,0),
	glm::ivec3(0 ,0 ,1),
	glm::ivec3(0 ,1 ,0)
};

uint opposing_face[6] = {
	5,
	3,
	4,
	1,
	2,
	0
};

uint vertex_index(Grid& grid, glm::ivec3 pos) {
	return pos.x + pos.y * (grid.width + 1) + pos.z * (grid.width + 1) * (grid.height + 1);
}

vertex_handle add_grid_vertex(CFDVolume& volume, Grid& grid, glm::ivec3 grid_pos, bool* unique) {
	uint index = vertex_index(grid, grid_pos);
	vertex_handle& result = grid.vertices[index];

	if (result.id == -1) {
		float resolution = grid.resolution;
		glm::vec3 min = grid.min;
		glm::vec3 pos = { min.x + grid_pos.x * resolution, min.y + grid_pos.y * resolution, min.z + grid_pos.z * resolution };

		result = { (int)volume.vertices.length };
		volume.vertices.append({ pos });
		*unique = true;
	}
	else {
		*unique = false;
	}

	assert(result.id != 16843009);
	return result;
}

float frand() {
	return (float)rand() / INT_MAX * 2.0 - 1.0;
}

void init_grid(Grid& grid, glm::ivec3 offset, glm::ivec3 size, vector<vertex_handle>* boundary_verts, vector<Boundary>* boundary, float resolution) {
	grid = {};
	grid.resolution = resolution;

	printf("Initialized grid\n");
	printf("Min %i %i %i\n", offset.x, offset.y, offset.z);
	printf("Size %i %i %i\n", size.x, size.y, size.z);

	grid.boundary_verts = boundary_verts;
	grid.boundary = boundary;

	grid.imin = offset;
	grid.imax = offset + size;
	grid.min = glm::vec3(offset) * resolution;
	grid.max = glm::vec3(grid.imax) * resolution;
	grid.width = size.x;
	grid.height = size.y;
	grid.depth = size.z;

	uint grid_cells = grid.width * grid.height * grid.depth;
	uint grid_cells_plus1 = (grid.width + 1) * (grid.height + 1) * (grid.depth + 1);
	grid.occupied = TEMPORARY_ZEROED_ARRAY(bool, grid_cells);
	grid.occupied2 = TEMPORARY_ZEROED_ARRAY(bool, grid_cells);

	//quite a large allocation, maybe a hashmap is more efficient, since it will be mostly empty
	grid.vertices = TEMPORARY_ZEROED_ARRAY(vertex_handle, grid_cells_plus1);
	grid.cell_ids = TEMPORARY_ARRAY(cell_handle, grid_cells);
}

void flood_fill(Grid& grid) {
	//Flood Fill to determine what's inside and out
	//Any Cell that is visited is outside if we start at the corner
	uint grid_cells = grid.width * grid.height * grid.depth;
	bool* occupied = grid.occupied;
	bool* out = grid.occupied2;

	for (uint i = 0; i < grid_cells; i++) {
		out[i] = true;
	}

	tvector<uint> stack;
	stack.append(0);
	out[0] = false; //todo assert corner is empty

	while (stack.length > 0) {
		uint index = stack.pop();
		glm::ivec3 pos = grid.pos(index);

		for (uint i = 0; i < 6; i++) {
			glm::ivec3 p = pos + grid_offsets[i];
			if (!grid.is_valid(p)) continue;

			uint neighbor_index = grid.index(p);
			if (!occupied[neighbor_index] && out[neighbor_index]) {
				out[neighbor_index] = false;
				stack.append(neighbor_index);
			}
		}
	}
}

void rasterize_surface(Grid& grid, SurfaceTriMesh& surface, vec3 spacing) {
	//RASTERIZE OUTLINE TO GRID
	for (tri_handle tri : surface) {
		//CFDCell& cell = mesh.cells[cell_id];
		AABB aabb;
		uint verts = 3; // shapes[cell.type].num_verts;
		for (uint i = 0; i < verts; i++) {
			vec3 pos = surface.position(tri, i);
			aabb.update(pos);
		}

		aabb.min -= glm::vec3(spacing);
		aabb.max += glm::vec3(spacing);

		//RASTERIZE AABB
		glm::ivec3 min_i = glm::ivec3(glm::floor((aabb.min - grid.min) / grid.resolution));
		glm::ivec3 max_i = glm::ivec3(glm::ceil((aabb.max - grid.min) / grid.resolution));

		for (uint z = min_i.z; z < max_i.z; z++) {
			for (uint y = min_i.y; y < max_i.y; y++) {
				for (uint x = min_i.x; x < max_i.x; x++) {
					grid[glm::ivec3(x, y, z)] = true;
				}
			}
		}
	}

	
	flood_fill(grid);
}

glm::ivec3 vertex_offset[8] = {
	{0, 0, 1},
	{1, 0, 1},
	{1, 0, 0},
	{0, 0, 0},

	{0, 1, 1},
	{1, 1, 1 },
	{1, 1, 0 },
	{0, 1, 0 }
};

void add_cell(CFDVolume& mesh, Grid& grid, glm::ivec3 pos, bool is_unique[8]) {
	CFDCell cell;
	cell.type = CFDCell::HEXAHEDRON;
	cell_handle curr = { (int)mesh.cells.length };

	//todo: counter-clockwise
	for (uint i = 0; i < 8; i++) {
		cell.vertices[i] = add_grid_vertex(mesh, grid, pos + vertex_offset[i], is_unique + i);
	}

	for (uint i = 0; i < 6; i++) {
		glm::ivec3 p = pos + grid_offsets[i];
		if (grid.is_valid(p)) {
			cell_handle neighbor = grid.cell_ids[grid.index(p)];
			if (neighbor.id != -1) {
				cell.faces[i].neighbor = neighbor; //mantain doubly linked connections
				mesh[neighbor].faces[opposing_face[i]].neighbor = curr;
			}
		}
		else {
			cell.faces[i].neighbor.id = -1;
		}
	}

	uint index = grid.index(pos);
	grid.occupied2[index] = true; //should this be a parameter?
	grid.cell_ids[index] = curr;

	mesh.cells.append(cell);
}

void add_cell(CFDVolume& mesh, Grid& grid, glm::ivec3 pos) {
	bool is_unique[8];
	add_cell(mesh, grid, pos, is_unique);
}

void rasterize_grid_to_grid(CFDVolume& mesh, Grid& out, Grid& in) {
	assert(out.resolution == 2 * in.resolution);

	glm::ivec3 min = in.imin / 2 - out.imin;
	glm::ivec3 max = out.imax - in.imin / 2;
	printf("Offset %i %i %i", min.x, min.y, min.z);

	for (uint z = 0; z < in.depth; z++) {
		for (uint y = 0; y < in.height; y++) {
			for (uint x = 0; x < in.width; x++) {
				out[glm::ivec3(x,y,z)/2 + min] |= in[glm::ivec3(x, y, z)];
			}
		}
	}

	//todo doing some redundant work, should be z+min and depth-max
	for (uint z = 0; z < out.depth; z++) {
		for (uint y = 0; y < out.height; y++) {
			for (uint x = 0; x < out.width; x++) {
				if (!out[glm::ivec3(x, y, z)]) continue;

				//todo insert into boundary
				for (uint z2 = 0; z2 < 2; z2++) {
					for (uint y2 = 0; y2 < 2; y2++) {
						for (uint x2 = 0; x2 < 2; x2++) {
							glm::ivec3 pos = (glm::ivec3(x, y, z) - min) * 2 + glm::ivec3(x2, y2, z2);
							if (!in.is_valid(pos)) continue;

							bool filled = in[pos];
							if (filled) continue;

							/*bool neighbor[8];
							for (uint i = 0; i < 6; i++) {
								glm::ivec3 p = pos + grid_offsets[i];
								neighbor[i] = grid.is_valid(p);
							}*/
							add_cell(mesh, in, pos);
						}
					}
				}
			}
		}
	}

	std::swap(out.occupied, out.occupied2);
}

//todo use 32 bit packing trick
void generate_contour(CFDVolume& mesh, Grid& grid, uint layers, bool* boundary_vert) {
	cell_handle* cell_ids = grid.cell_ids;

	std::swap(grid.occupied, grid.occupied2);


	//GENERATE OUTLINE
	for (uint n = 0; n < layers; n++) {
		bool* in = grid.occupied;
		bool* out = grid.occupied2;

		for (uint z = 0; z < grid.depth; z++) {
			for (uint y = 0; y < grid.height; y++) {
				for (uint x = 0; x < grid.width; x++) {
					glm::ivec3 pos = glm::ivec3(x, y, z);

					uint index = grid.index(pos);
					bool is_filled = in[index];
					bool fill = false;

					bool neighbor[6] = {};

					if (!is_filled) {
						for (uint i = 0; i < 6; i++) {
							glm::ivec3 p = pos + grid_offsets[i];
							if (grid.is_valid(p)) {
								bool boundary = grid[p];
								neighbor[i] = boundary;
								fill |= boundary;
							}
						}
					}

					//if (n == layers) fill = fill && (z % 2 != 0 || y % 2 != 0 || x %2 != 0);

					if (fill) {
						bool is_boundary_layer = n == 0 && boundary_vert;
			
						add_cell(mesh, grid, pos);
						CFDCell& cell = mesh.cells.last();

						if (is_boundary_layer) {
							for (uint i = 0; i < 6; i++) {
								if (!neighbor[i]) continue;

								Boundary face;
								face.cell = { (int)mesh.cells.length };
								for (uint j = 0; j < 4; j++) {
									uint index = hexahedron_shape[i][j];
									vertex_handle v = cell.vertices[index];
									face.vertices[j] = v;

									glm::ivec3 vert_pos = pos + vertex_offset[index];

									uint vindex = vertex_index(grid, vert_pos);
									if (!boundary_vert[vindex]) {
										grid.boundary_verts->append(v);
										boundary_vert[vindex] = true;
									}
								}

								grid.boundary->append(face);
							}

						}

					}
					else {
						out[index] = is_filled;
					}
				}
			}
		}

		std::swap(grid.occupied, grid.occupied2);
	}
}

void fill_volume(CFDVolume& volume, Grid& grid) {
	std::swap(grid.occupied, grid.occupied2);
	for (uint z = 0; z < grid.depth; z++) {
		for (uint y = 0; y < grid.height; y++) {
			for (uint x = 0; x < grid.width; x++) {
				glm::ivec3 pos = glm::ivec3(x, y, z);
				if (grid[pos]) continue;
				add_cell(volume, grid, pos);
			}
		}
	}


}

void inflate_and_align_aabb(glm::ivec3& min, glm::ivec3& max, uint layers) {
	min -= layers;
	max += layers;
	
	min.x -= abs(min.x % 2);
	min.y -= abs(min.y % 2);
	min.z -= abs(min.z % 2);
	max.x += abs(max.x % 2);
	max.y += abs(max.y % 2);
	max.z += abs(max.z % 2);
}

bool adjust_to_domain(const AABB& domain_bounds, glm::ivec3& min, glm::ivec3& max, float resolution) {
	glm::ivec3 domain_min = glm::floor(domain_bounds.min / resolution);
	glm::ivec3 domain_max = glm::ceil(domain_bounds.max / resolution);

	inflate_and_align_aabb(domain_min, domain_max, 0);

	min = glm::max(min, domain_min);
	max = glm::min(max, domain_max);

	return min == domain_min && max == domain_max;
}

void build_grid(CFDVolume& mesh, SurfaceTriMesh& surface, const AABB& domain_bounds, vector<vertex_handle>& boundary_verts, vector<Boundary>& boundary, float resolution, uint layers) {
	LinearRegion region(get_temporary_allocator());

	AABB aabb;
	for (uint i = 0; i < surface.positions.length; i++) {
		aabb.update(surface.positions[i]);
	}

    glm::vec3 spacing = glm::vec3(0); //glm::vec3(2.0f * resolution);

	aabb.min -= spacing;
	aabb.max += spacing;


	glm::ivec3 min = glm::floor(aabb.min / resolution);
	glm::ivec3 max = glm::ceil(aabb.max / resolution);

	inflate_and_align_aabb(min, max, layers);

	bool last = adjust_to_domain(domain_bounds, min, max, resolution);

	Grid grid;
	init_grid(grid, min, max - min, &boundary_verts, &boundary, resolution);

	bool* boundary_vert = TEMPORARY_ZEROED_ARRAY(bool, (grid.width+1) * (grid.height+1) * (grid.depth+1));

	rasterize_surface(grid, surface, spacing);
	
	if (last) fill_volume(mesh, grid);
	else generate_contour(mesh, grid, layers, boundary_vert);

	vector<vertex_handle> boundary_verts_between_layers;
	vector<Boundary> boundary_between_layers;

	//todo reclaim memory
	while (!last) {
		resolution *= 2;
		//layers *= 2;

		min = glm::floor(glm::vec3(min) / 2.0f);
		max = glm::ceil(glm::vec3(max) / 2.0f);

		inflate_and_align_aabb(min, max, layers);
		last = adjust_to_domain(domain_bounds, min, max, resolution);

		Grid higher;
		init_grid(higher, min, max - min, &boundary_verts_between_layers, &boundary_between_layers, resolution);

		rasterize_grid_to_grid(mesh, higher, grid);
		if (last) fill_volume(mesh, higher);
		else generate_contour(mesh, higher, layers, nullptr);
		
		grid = higher;
	}
}
