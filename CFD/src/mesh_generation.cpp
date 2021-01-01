#include <glm/vec3.hpp>
#include "core/core.h"

//found on https://cs.stackexchange.com/questions/37952/hash-function-floating-point-inputs-for-genetic-algorithm
u64 hash_func(glm::vec3 position) {
    int h = 1;
    for (int i = 0; i < 3; i++) {
        union {
            float as_float;
            int as_int;
        } value = { position[i] };
        
        h = 31 * h + value.as_int;
    }
    h ^= (h >> 20) ^ (h >> 12);
    return h ^ (h >> 7) ^ (h >> 4);
}

#include "cfd_ids.h"
#include "mesh.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include "components.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include <algorithm>



void get_positions(slice<CFDVertex> vertices, const CFDPolygon& polygon, vec3* positions) {
	uint verts = polygon.type;
	for (uint i = 0; i < verts; i++) {
		positions[i] = vertices[polygon.vertices[i].id].position;
	}
}

void get_positions(slice<CFDVertex> vertices, slice<vertex_handle> handles, vec3* positions) {
	for (uint i = 0; i < handles.length; i++) {
		positions[i] = vertices[handles[i].id].position;
	}
}

template<typename T>
using spatial_hash_map = hash_map_base<vec3, T>;

template<typename T>
spatial_hash_map<T> make_t_hash_map(uint hash_map_size) {
	return spatial_hash_map<T>(hash_map_size,
		TEMPORARY_ZEROED_ARRAY(hash_meta, hash_map_size),
		TEMPORARY_ZEROED_ARRAY(vec3, hash_map_size),
		TEMPORARY_ZEROED_ARRAY(T, hash_map_size)
	);
}

//essentially a precomputed modulus
const uint vert_for_edge[3][3] = {
	{0, 1, 2},
	{1, 2, 0},
	{2, 0, 1}
};

const uint quad_winding[3][4] = {
	{0,3,1,2},
	{0,1,3,2},
	{0,1,2,3},
};

template<typename T>
void form_cc_quad(T* out, const T* in, uint edge_point) {
	/*uint quad_winding[4] = { 0,1,2,3 };
	glm::vec3 center;
	for (uint i = 0; i < 4; i++) {
		center += points[i];
	}
	center /= 4;

	std::sort(quad_winding, quad_winding + 4, [&](uint a, uint b) {
		float angle1 = acosf(glm::dot(points[a] - center, glm::vec3(0, 1, 0)));
		float angle2 = acosf(glm::dot(points[b] - center, glm::vec3(0, 1, 0)));
		return angle1 > angle2; //Counter-Clockwise
	});*/
	
	for (uint i = 0; i < 4; i++) {
		out[i] = in[quad_winding[edge_point][i]];
	}
}

bool check_for_duplicates(CFDPolygon& polygon) {
	uint num_verts = polygon.type;
	for (uint i = 0; i < num_verts; i++) {
		for (uint j = i + 1; j < num_verts; j++) {
			if (polygon.vertices[i].id == polygon.vertices[j].id) {
				return true;
			}
		}
	}

	return false;
}

float quad_score(vec3 p[4]) {
	float parallel_lines_weighting = 0.2;
	float perpendicular_corners_weighting = 0.8;

	float parallel1 = dot(normalize(p[2] - p[1]), normalize(p[3] - p[0]));
	float parallel2 = dot(normalize(p[0] - p[1]), normalize(p[3] - p[2]));
	float perpendicular1 = dot(normalize(p[2] - p[1]), normalize(p[2] - p[3]));
	float perpendicular2 = dot(normalize(p[0] - p[1]), normalize(p[0] - p[3]));

	float perpendicular = 1 - 0.5*fabs(perpendicular1) - 0.5*fabs(perpendicular2);
	float parallel = 0.5*fabs(parallel1) + 0.5*fabs(parallel2);

	return perpendicular * perpendicular_corners_weighting + parallel_lines_weighting * parallel;
}

struct QuadSelectionInfo { 
	float edge_score[3];
};

struct TopScore {
	float score = 0;
	polygon_handle id;
};

struct FollowEdgeLoopJob {

};

CFDPolygon form_quad(CFDSurface& surface, polygon_handle p_id, uint connect_with_edge) {
	const CFDPolygon& polygon = surface.polygons[p_id.id];

	const CFDPolygon::Edge& edge = polygon.edges[connect_with_edge];
	const CFDPolygon& connect_with = surface.polygons[edge.neighbor.id];

	vertex_handle vertices[4];
	memcpy_t(vertices, polygon.vertices, 3);
	vertices[3] = connect_with.vertices[(edge.neighbor_edge + 2) % 3];

	vec3 positions[4];
	get_positions(surface.vertices, polygon, positions);
	positions[3] = surface.vertices[vertices[3].id].position;

	CFDPolygon quad{ CFDPolygon::QUAD };
	form_cc_quad(quad.vertices, vertices, connect_with_edge);

	//todo find smarter way of doing this
	//this is to unblock myself

	for (uint i = 0; i < 4; i++) {
		vertex_handle v_a = quad.vertices[i % 4];
		vertex_handle v_b = quad.vertices[(i + 1) % 4];

		for (uint j = 0; j < 3; j++) {
			vertex_handle v_a2 = polygon.vertices[j % 3];
			vertex_handle v_b2 = polygon.vertices[(j + 1) % 3];

			if ((v_a.id == v_a2.id && v_b.id == v_b2.id) || (v_a.id == v_b2.id && v_b.id == v_a2.id)) {
				quad.edges[i] = polygon.edges[j];
				break;
			}
		}

		if (quad.edges[i].neighbor.id != -1) continue;

		for (uint j = 0; j < 3; j++) {
			vertex_handle v_a2 = connect_with.vertices[j % 3];
			vertex_handle v_b2 = connect_with.vertices[(j + 1) % 3];

			if ((v_a.id == v_a2.id && v_b.id == v_b2.id) || (v_a.id == v_b2.id && v_b.id == v_a2.id)) {
				quad.edges[i] = connect_with.edges[j];
				break;
			}
		}
	}

	return quad;
}

int find_best_quad_edge(polygon_handle* new_ids, CFDPolygon& polygon, QuadSelectionInfo& info, float min_score = 0.0f) {
	int connect_with_edge = -1;
	float highest_score = min_score;
	for (uint j = 0; j < 3; j++) {
		polygon_handle neighbor = polygon.edges[j].neighbor;
		if (new_ids[neighbor.id].id != -1) continue;

		float score = info.edge_score[j];
		if (score > highest_score) {
			connect_with_edge = j;
			highest_score = score;
		}
	}
	return connect_with_edge;
}

bool add_quad(CFDPolygon& quad, vector<CFDPolygon>& polygons, polygon_handle* new_ids, CFDSurface& surface, polygon_handle handle, int connect_with) {
	if (connect_with == -1) return false;

	CFDPolygon& polygon = surface.polygons[handle.id];
	polygon_handle neighbor = polygon.edges[connect_with].neighbor;
	if (neighbor.id == -1) return false;
	if (new_ids[neighbor.id].id != -1) return false;
	quad = form_quad(surface, handle, connect_with);
	
	int id = polygons.length;
	new_ids[handle.id].id = id;
	new_ids[neighbor.id].id = id;
	polygons.append(quad);

	return true;
}

vec3 get_edge_dir(CFDSurface& surface, CFDPolygon& polygon, uint edge_index) {
	uint a = polygon.vertices[edge_index].id;
	uint b = polygon.vertices[(edge_index + 1) % 3].id;

	return normalize(surface.vertices[a].position - surface.vertices[b].position);
}

int find_best_quad_edge_from(CFDSurface& surface, polygon_handle* new_ids, CFDPolygon& polygon, uint edge_index, vec3 edge_dir, int min_score = 0.0f) {
	float diagonal_dot = min_score;
	int chosen_edge = -1;
	for (uint i = 0; i < 3; i++) {
		if (i == edge_index) continue;
		if (new_ids[polygon.edges[i].neighbor.id].id != -1) continue;
		uint a = polygon.vertices[i].id;
		uint b = polygon.vertices[(i + 1) % 3].id;
		vec3 dir = normalize(surface.vertices[a].position - surface.vertices[b].position);
		float d = dot(edge_dir, dir);
		//float score = infos[p.id].edge_score[chosen_edge];
		if (fabs(d) > diagonal_dot) {
			diagonal_dot = fabs(d);
			chosen_edge = i;
		}
	}

	return chosen_edge;
}

void scan_edgeloop(vector<CFDPolygon>& polygons, CFDSurface& surface, polygon_handle* new_ids, QuadSelectionInfo* infos, CFDPolygon& quad, uint starting_edge) {
	polygon_handle p = quad.edges[starting_edge].neighbor;

	uint count = 0;
	uint edge_index = quad.edges[starting_edge].neighbor_edge;

	while (p.id != -1) {
		CFDPolygon& polygon = surface.polygons[p.id];
		
		glm::vec3 edge_dir = get_edge_dir(surface, polygon, edge_index);
		int chosen_edge = find_best_quad_edge_from(surface, new_ids, polygon, edge_index, edge_dir);

		float score = infos[p.id].edge_score[chosen_edge];
		//printf("Score of connecting edge %i, starting from (%i): %f\n", chosen_edge, edge_index, score);
		if (score < 0.5) break;

		//int chosen_edge = find_best_quad_edge(new_ids, polygon, infos[p.id]);

		CFDPolygon quad;
		if (!add_quad(quad, polygons, new_ids, surface, p, chosen_edge)) break;
		//if (edge_loops == 8) polygons[polygons.length - 1].type = CFDPolygon::TRIANGLE;

		CFDPolygon& neighbor = surface.polygons[polygon.edges[chosen_edge].neighbor.id];
		uint chosen_edge_neighbor = polygon.edges[chosen_edge].neighbor_edge;

		int best_edge = find_best_quad_edge_from(surface, new_ids, neighbor, chosen_edge_neighbor, edge_dir, 0.5f);
		if (best_edge != -1) {
			edge_index = neighbor.edges[best_edge].neighbor_edge;
			p = neighbor.edges[best_edge].neighbor;
		}
		else {
			p.id = -1;
		}

		count++;
		//printf("Created quad! %i %i %i %i\n", quad.vertices[0].id, quad.vertices[1].id, quad.vertices[2].id, quad.vertices[3].id);
	}
}

CFDSurface quadify_surface(CFDSurface& surface) {
	QuadSelectionInfo* infos = TEMPORARY_ZEROED_ARRAY(QuadSelectionInfo, surface.polygons.length);
	TopScore* top_scores = TEMPORARY_ZEROED_ARRAY(TopScore, surface.polygons.length);

	for (uint i = 0; i < surface.polygons.length; i++) {
		CFDPolygon& poly = surface.polygons[i];
		if (poly.type != CFDPolygon::TRIANGLE) continue;

		vec3 combined_positions[4] = {};
		get_positions(surface.vertices, poly, combined_positions);
			
		top_scores[i].id.id = i;
		
		for (uint j = 0; j < 3; j++) {
			CFDPolygon::Edge& edge = poly.edges[j];
			if (edge.neighbor.id == -1) continue;
			CFDPolygon& neighbor = surface.polygons[edge.neighbor.id];
			if (neighbor.type != CFDPolygon::TRIANGLE) continue;

			vertex_handle opposing_vert = neighbor.vertices[(edge.neighbor_edge+2)%3];
			combined_positions[3] = surface.vertices[opposing_vert.id].position;
		
			vec3 p[4];
			form_cc_quad(p, combined_positions, j);

			CFDVertex& a = surface.vertices[poly.vertices[j].id];
			CFDVertex& b = surface.vertices[poly.vertices[(j + 1) % 3].id];
			float score = quad_score(p);
			infos[i].edge_score[j] = score;
			top_scores[i].score = glm::max(top_scores[i].score, score);
		}
	}

	std::sort(top_scores, top_scores+surface.polygons.length, [](TopScore a, TopScore b) {
		return a.score < b.score;
	});
	
	polygon_handle* new_ids = TEMPORARY_ZEROED_ARRAY(polygon_handle, surface.polygons.length);
	vector<CFDPolygon> polygons;

	uint edge_loops = 0;
	for (int i = surface.polygons.length - 1; i >= 0; i--) {
		//if (edge_loops == 8) break;

		TopScore top_score = top_scores[i];
		if (new_ids[top_score.id.id].id != -1) continue;

		CFDPolygon quad;

		int connecting_edge = find_best_quad_edge(new_ids, surface.polygons[top_score.id.id], infos[top_score.id.id]);
		if (!add_quad(quad, polygons, new_ids, surface, top_score.id, connecting_edge)) continue;

		edge_loops++;

		//printf("Created quad!\n");

		float longest = 0;
		uint crawl_from = 0;
		for (uint i = 0; i < 2; i++) {
			glm::vec3 a = surface.vertices[quad.vertices[i].id].position;
			glm::vec3 b = surface.vertices[quad.vertices[(i+1)%4].id].position;

			glm::vec3 length3 = a - b;
			float length = glm::dot(length3, length3);
			if (length > longest) {
				longest = length;
				crawl_from = i;
			}
		}

		//Scan up and down edge loop
		for (uint dir = 0; dir < 2; dir++) {
			uint starting_edge = crawl_from + 2*dir;
			polygon_handle p = quad.edges[starting_edge].neighbor;

			uint edge_index = quad.edges[starting_edge].neighbor_edge;
			scan_edgeloop(polygons, surface, new_ids, infos, quad, edge_index);
		}

		
		//Create adjacent quad and then scan those edge loops
		for (uint dir = 0; dir < 2; dir++) {
			uint starting_edge = !crawl_from + 2 * dir;
			
			CFDPolygon adjacent_quad;
			//add_quad(adjacent_quad, polygons, new_ids, surface, quad.edges[starting_edge].neighbor, diagonal_edge);
		}
		
		//break;
	}

	for (uint i = 0; i < surface.polygons.length; i++) {
		if (new_ids[i].id != -1) continue;
		CFDPolygon polygon = surface.polygons[i];
		new_ids[i].id = polygons.length;
		polygons.append(polygon);
	}

	for (CFDPolygon& polygon : polygons) {
		uint num_verts = polygon.type;
		for (uint j = 0; j < num_verts; j++) {
			if (polygon.edges[j].neighbor.id == -1) continue;
			polygon.edges[j].neighbor = new_ids[polygon.edges[j].neighbor.id];
		}
	}

	return {std::move(polygons), surface.vertices};
}

struct VertexInfo {
	uint id;
	vec3 position;
	vec3 normal;
	uint count;
};

struct EdgeInfo {
	polygon_handle neighbor;
	uint neighbor_edge;
};


vec3 triangle_normal(vec3 positions[3]) {
	vec3 dir1 = normalize(positions[0] - positions[1]);
	vec3 dir2 = normalize(positions[0] - positions[2]);
	return normalize(cross(dir1, dir2));
}

vec3 quad_normal(vec3 positions[4]) {
	vec3 triangle1[3] = { positions[0], positions[1], positions[2] };
	vec3 triangle2[3] = { positions[0], positions[2], positions[3] };
	vec3 normal1 = triangle_normal(triangle1);
	vec3 normal2 = triangle_normal(triangle2);
	return normalize((normal1 + normal2) / 2.0f);
}

CFDSurface surface_from_mesh(vector<CFDVertex>& vertices, const glm::mat4& mat, Mesh& mesh) {
	uint lod = 0;
	slice<uint> mesh_indices = mesh.indices[lod];
	slice<Vertex> mesh_vertices = mesh.vertices[lod];

	uint hash_map_size = mesh_indices.length * 2;

	spatial_hash_map<VertexInfo> vertex_hash_map = make_t_hash_map<VertexInfo>(hash_map_size);
	spatial_hash_map<EdgeInfo> edge_hash_map = make_t_hash_map<EdgeInfo>(hash_map_size);

	uint vertex_id = 0;
	float tolerance = 0.001;

	vector<CFDPolygon> triangles;
	triangles.reserve(mesh_indices.length / 3);

	for (uint i = 0; i < mesh_indices.length; i += 3) {
		uint* indices = mesh_indices.data + i;
		uint triangle_id = i / 3;

		CFDPolygon triangle{ CFDPolygon::TRIANGLE };

		vec3 vertex_positions[3] = {};

		for (uint j = 0; j < 3; j++) {
			Vertex vertex = mesh.vertices[lod][indices[j]];

			glm::vec3 position = vertex.position;
			position /= tolerance;
			position = glm::ceil(position);
			position *= tolerance;

			VertexInfo& info = vertex_hash_map[position];
			uint previous_count = info.count++;
			if (previous_count == 0) { //assign the vertex an ID
				info.id = vertex_id++;
			}
			
			info.normal += vertex.normal;

			triangle.vertices[j].id = info.id;
			vertex_positions[j] = vertex.position;
		}

		/*assert(triangle.vertices[0].id != triangle.vertices[1].id
			&& triangle.vertices[1].id != triangle.vertices[2].id
			&& triangle.vertices[2].id != triangle.vertices[0].id
		);*/

		for (uint j = 0; j < 3; j++) {
			uint a = vert_for_edge[j][0];
			uint b = vert_for_edge[j][1];

			vec3 mid_point = (vertex_positions[a] + vertex_positions[b]) / 2.0f;

			uint index = edge_hash_map.add(mid_point);
			EdgeInfo& info = edge_hash_map.values[index];

			if (info.neighbor.id == -1) {
				info.neighbor.id = triangle_id;
				info.neighbor_edge = j;
			}
			else {
				CFDPolygon::Edge& edge = triangle.edges[j];
				CFDPolygon::Edge& neighbor_edge = triangles[info.neighbor.id].edges[info.neighbor_edge];

				edge.neighbor = info.neighbor;
				edge.neighbor_edge = info.neighbor_edge;
				neighbor_edge.neighbor.id = triangle_id;
				neighbor_edge.neighbor_edge = j;
			}
		}

		triangles.append(triangle);
	}

	vertices.resize(vertex_id);

	for (auto [position, info] : vertex_hash_map) {
		vertices[info.id].position = glm::vec3(mat * glm::vec4(glm::vec3(position),1.0));
		vertices[info.id].normal = glm::normalize(glm::mat3(mat) * info.normal);
	}

	return {std::move(triangles), vertices};
}


void create_boundary(CFDVolume& result, uint& extruded_vertices_offset, Mesh& mesh, const glm::mat4& model_m, float dist) {
	result = {};

	vector<CFDVertex> vertices;
	CFDSurface surface = surface_from_mesh(vertices, model_m, mesh);
	surface = quadify_surface(surface);

	extruded_vertices_offset = vertices.length;
	
	result.vertices.resize(vertices.length * 2);
	for (uint i = 0; i < vertices.length; i++) {
		result.vertices[i] = vertices[i];
		//extruded vert
		result.vertices[i + extruded_vertices_offset].position = vertices[i].position + dist * vertices[i].normal;
		result.vertices[i + extruded_vertices_offset].normal = vertices[i].normal;
	}

	result.cells.resize(surface.polygons.length);

	for (uint i = 0; i < surface.polygons.length; i++) {
		CFDPolygon& polygon = surface.polygons[i];
		
		CFDCell& cell = result.cells[i];

		cell.type = polygon.type == CFDPolygon::TRIANGLE ? CFDCell::TRIPRISM : CFDCell::HEXAHEDRON;

		uint sides = polygon.type;
		for (uint j = 0; j < sides; j++) {
			cell.vertices[j] = polygon.vertices[j];
			cell.vertices[j + sides].id = polygon.vertices[j].id + extruded_vertices_offset;
		}

		//bottom cell.faces[0]
		//top cell.faces[polygon.type]
		uint neighbors = polygon.type;
		for (uint j = 0; j < neighbors; j++) {
			cell.faces[j + 1].neighbor.id = polygon.edges[j].neighbor.id;
		}
	}
}

void extrude_contour_mesh(CFDVolume& mesh, uint& extruded_vertices_offset, uint& extruded_cell_offset, float dist) {
	uint vertex_count = mesh.vertices.length;
	for (uint i = extruded_vertices_offset; i < vertex_count; i++) {
		CFDVertex& vertex = mesh.vertices[i];
		CFDVertex extruded = vertex;
		extruded.position += vertex.normal * dist;

		mesh.vertices.append(extruded);
	}
	uint offset = vertex_count - extruded_vertices_offset;
	extruded_vertices_offset = vertex_count;

	uint cell_count = mesh.cells.length;
	uint cell_offset = cell_count - extruded_cell_offset;

	for (uint i = extruded_cell_offset; i < cell_count; i++) {
		CFDCell& cell = mesh.cells[i];

		CFDCell extruded_cell;
		extruded_cell.type = cell.type;

		switch (cell.type) {
		case CFDCell::HEXAHEDRON: {
			memcpy_t(extruded_cell.vertices, cell.vertices + 4, 4);
			for (uint j = 0; j < 4; j++) {
				extruded_cell.vertices[j + 4].id = extruded_cell.vertices[j].id + offset;
				extruded_cell.faces[j + 1].neighbor.id = cell.faces[j + 1].neighbor.id + cell_offset;
			}

			extruded_cell.faces[0].neighbor.id = i;
			cell.faces[5].neighbor.id = mesh.cells.length;

			break;
		}

		case CFDCell::TRIPRISM: {
			memcpy_t(extruded_cell.vertices, cell.vertices + 3, 3);
			for (uint j = 0; j < 3; j++) {
				extruded_cell.vertices[j + 3].id = extruded_cell.vertices[j].id + offset;
				extruded_cell.faces[j + 1].neighbor.id = cell.faces[j + 1].neighbor.id + cell_offset;
			}

			extruded_cell.faces[0].neighbor.id = i;
			cell.faces[4].neighbor.id = mesh.cells.length;

			break;
		}
		}

		mesh.cells.append(extruded_cell);
	}
	extruded_cell_offset = cell_count;
	printf("Generated %i new cells, offset %i, dist %f\n", mesh.cells.length - extruded_cell_offset, offset, dist);
}

float sq_distance(glm::vec3 a, glm::vec3 b) {
	glm::vec3 dist3 = a - b;
	return glm::dot(dist3, dist3);
}

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
			for (uint i = 0; i < 8; i++) children[i] = {};
			for (uint i = 0; i < MAX_PER_CELL; i++) {
				aabbs[i] = {};
				cells[i] = {};
			}
			free_next = nullptr;
		}
	};

	struct CellInfo {
		Subdivision* subdivision;
		uint index;
	};

	struct Result {
		cell_handle cell;
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
		} else {
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

	Result find_closest(Subdivision& subdivision, AABB& aabb) {
		Result result;
		if (!aabb.intersects(subdivision.aabb)) return result;

		glm::vec3 center = aabb.centroid();

		bool leaf = is_leaf(subdivision);
		if (leaf) {
			last_visited = &subdivision;

			for (uint i = 0; i < subdivision.count; i++) {
				cell_handle cell_handle = subdivision.p->cells[i];
				CFDCell& cell = cells[cell_handle.id];

				//todo could skip this, if center is further than some threschold
				
				uint verts = shapes[cell.type].num_verts;
				for (uint i = 0; i < verts; i++) {
					vertex_handle vertex_handle = cell.vertices[i];
					vec3 position = vertices[vertex_handle.id].position;
					float dist = length(position - center);

					if (dist < result.dist) {
						result.dist = dist;
						result.cell = cell_handle;
						result.vertex = vertex_handle;
					}
				}
			}
		} else {
			for (uint i = 0; i < 8; i++) {
				Result child_result = find_closest(subdivision.p->children[i], aabb);
				if (child_result.dist < result.dist) result = child_result;
			}
		}

		return result;
	}

	Result find_closest(glm::vec3 position, float radius) {
		uint depth = 0;

		AABB sphere_aabb;
		sphere_aabb.min = position - glm::vec3(radius);
		sphere_aabb.max = position + glm::vec3(radius);

		Subdivision* start_from = last_visited;

		while (!sphere_aabb.inside(start_from->aabb_division)) {
			if (!start_from->parent) break; //found root
			start_from = start_from->parent;
		}

		Result result = find_closest(*start_from, sphere_aabb);
		if (result.dist < radius) {
			return result;
		}
		else {
			return {};
		}
	}

	uint centroid_to_index(glm::vec3 centroid, glm::vec3 min, glm::vec3 half_size) {
		glm::vec3 vec = (centroid - min + FLT_EPSILON) / (half_size + 2*FLT_EPSILON);
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
					{glm::vec3(min.x+half_size.x, min.y, min.z)}, //right front
					{glm::vec3(min.x,             min.y, min.z + half_size.z)}, //left back
					{glm::vec3(min.x+half_size.x, min.y, min.z + half_size.z)}, //right back
					//top
					{glm::vec3(min.x,             min.y+half_size.y, min.z)}, //left front
					{glm::vec3(min.x+half_size.x, min.y+half_size.y, min.z)}, //right front
					{glm::vec3(min.x,             min.y+half_size.y, min.z+half_size.z)}, //left back
					{glm::vec3(min.x+half_size.x, min.y+half_size.y, min.z+half_size.z)}, //right back
				};

				for (uint i = 0; i < 8; i++) {
					init(divisions[i]);
					divisions[i].aabb_division = {mins[i], mins[i] + half_size};
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

	Front::Result result = front.find_closest(glm::vec3(0,1.0,0.0), 0.8);
	assert(result.cell.id == 0);
	assert(result.vertex.id == 4);
}

void swap_vertex(vertex_handle& a, vertex_handle& b) {
	vertex_handle tmp = a;
	a = b;
	b = tmp;
}

void sort3_vertices(vertex_handle verts[3]) {
	if (verts[0].id > verts[1].id) swap_vertex(verts[0], verts[1]);
	if (verts[1].id > verts[2].id) swap_vertex(verts[1], verts[2]);
	if (verts[0].id > verts[1].id) swap_vertex(verts[0], verts[1]);
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

void create_isotropic_cell(CFDVolume& mesh, Front& front, CFDCell& cell, vec3* positions, glm::vec3 normal) {
	const ShapeDesc& shape = shapes[cell.type];
	uint base_sides = shape.faces[0].num_verts;

	vec3 centroid;
	for (uint i = 0; i < base_sides; i++) centroid += positions[i];
	centroid /= base_sides;

	//length(positions[i] - positions[(i + 1) % base_sides])
	float r_h = sqrtf(3.0) / 2.0;
	float r = 0.0f;
	for (uint i = 0; i < base_sides; i++) r += r_h*length(positions[i] - centroid);
	r /= base_sides;


	glm::vec3 position = centroid + glm::normalize(normal) * r;

	Front::Result result = front.find_closest(position, r);
	vertex_handle vertex = result.vertex;

	//Add cell to the front and mesh
	int cell_id = mesh.cells.length;

	if (vertex.id == -1) {
		vertex.id = mesh.vertices.length;
		cell.vertices[base_sides] = vertex;
		mesh.vertices.append({ position, normal });

		goto add_cell;
	}
	else {
		cell.vertices[base_sides] = vertex;
		CFDCell& neighbor = mesh.cells[result.cell.id];

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
				faces_cell[i][j] = neighbor.vertices[neighbor_shape.faces[i].verts[j]];
			}
			sort3_vertices(faces_cell[i]);
		}

		for (uint i = 0; i < shape.num_faces; i++) {
			for (uint j = 0; j < neighbor_shape.num_faces; j++) {
				if (memcmp(faces_cell + i, faces_neighbor + j, sizeof(vertex_handle) * 3) == 0) {
					cell.faces[i].neighbor = result.cell;
					neighbor.faces[i].neighbor = { cell_id };
					goto add_cell;
				}
			}
		}
	}

	add_cell:
		mesh.cells.append(cell);
		front.add_cell({ cell_id });
}

void advancing_front_triangulation(CFDVolume& mesh, Front& front, uint& extruded_vertices_offset, uint& extruded_cell_offset, float grid_size) {
	uint cell_count = mesh.cells.length;
	for (uint cell_id = extruded_cell_offset; cell_id < cell_count; cell_id++) {
		
		
		const ShapeDesc& shape = shapes[mesh.cells[cell_id].type];

		//assume base is already connected
		for (uint i = 1; i < shape.num_faces; i++) {
			CFDCell& cell = mesh.cells[cell_id];
			if (cell.faces[i].neighbor.id == -1) {
				const ShapeDesc::Face& face = shape.faces[i];

				vertex_handle vertices[4];
				vec3 positions[4];

				uint verts = face.num_verts;
				for (uint j = 0; j < face.num_verts; j++) {
					vertices[j] = cell.vertices[face.verts[j]];
					positions[j] = mesh.vertices[vertices[j].id].position;
				}

				CFDCell new_cell;
				vec3 normal;
				if (verts == 4) {
					new_cell.type = CFDCell::PENTAHEDRON;
					normal = quad_normal(positions);
				}
				else {
					new_cell.type = CFDCell::TETRAHEDRON;
					normal = triangle_normal(positions);
				}

				memcpy_t(new_cell.vertices, vertices, verts);
				create_isotropic_cell(mesh, front, new_cell, positions, normal);
			}
		}
	}
}

CFDVolume generate_mesh(World& world, CFDMeshError& err) {
	auto some_mesh = world.first<Transform, CFDMesh>();
	auto some_domain = world.first<Transform, CFDDomain>();

	CFDVolume result;

	if (some_domain && some_mesh) {
		auto [mesh_entity, mesh_trans, mesh] = *some_mesh;
		auto [domain_entity, domain_trans, domain] = *some_domain;

		AABB domain_bounds;
		domain_bounds.min = domain_trans.position - domain.size;
		domain_bounds.max = domain_trans.position + domain.size;

		Model* model = get_Model(mesh.model);
		glm::mat4 model_m = compute_model_matrix(mesh_trans);

		AABB mesh_bounds = model->aabb.apply(model_m);

		if (!mesh_bounds.inside(domain_bounds)) {
			err.type = CFDMeshError::MeshOutsideDomain;
			err.id = mesh_entity.id;
			return result;
		}

		//glm::vec3 position = mesh_trans.position - domain_bounds.min;

		float initial = domain.contour_initial_thickness;
		float a = domain.contour_thickness_expontent;

		uint n = domain.contour_layers;
		uint extruded_vertice_watermark = 0;
		uint extruded_cells_watermark = 0;
		create_boundary(result, extruded_vertice_watermark, model->meshes[0], model_m, initial);

		for (uint i = 1; i < n; i++) {
			float dist = initial * pow(a, i);
			extrude_contour_mesh(result, extruded_vertice_watermark, extruded_cells_watermark, dist);
		}

		Front front(result.vertices, result.cells, domain_bounds);

		for (uint i = 0; i < domain.tetrahedron_layers; i++) {
			advancing_front_triangulation(result, front, extruded_vertice_watermark, extruded_cells_watermark, 0.1);
		}
	}
	else {
		err.type = CFDMeshError::NoMeshOrDomain;
	}

	return result;
}

void log_error(CFDMeshError& err) {
	switch (err.type) {
	case CFDMeshError::NoMeshOrDomain: 
		printf("[CFD Error] Found no mesh or domain\n");
		break;

	case CFDMeshError::MeshOutsideDomain:
		printf("[CFD Error] Mesh %i outside of domain", err.id);

	case CFDMeshError::None:
		break;
	}
}
