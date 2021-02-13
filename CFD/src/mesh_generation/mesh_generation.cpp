#include <glm/vec3.hpp>
#include "core/core.h"
#include "mesh_generation/delaunay_advancing_front.h"
#include "mesh_generation/delaunay.h"

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

int find_best_quad_edge_and_score(polygon_handle* new_ids, CFDPolygon& polygon, QuadSelectionInfo& info, float* highest_score) {
    int connect_with_edge = -1;
    for (uint j = 0; j < 3; j++) {
        polygon_handle neighbor = polygon.edges[j].neighbor;
        if (new_ids[neighbor.id].id != -1) continue;

        float score = info.edge_score[j];
        if (score > *highest_score) {
            connect_with_edge = j;
            *highest_score = score;
        }
    }
    return connect_with_edge;
}

int find_best_quad_edge(polygon_handle* new_ids, CFDPolygon& polygon, QuadSelectionInfo& info, float min_score = 0.0f) {
	float score = min_score;
    return find_best_quad_edge_and_score(new_ids, polygon, info, &score);
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

bool scan_edgeloop(vector<CFDPolygon>& polygons, CFDSurface& surface, polygon_handle* new_ids, QuadSelectionInfo* infos, CFDPolygon& quad, uint starting_edge) {
    polygon_handle p = quad.edges[starting_edge].neighbor;
    
    if (p.id == -1 || new_ids[p.id].id != -1) return false;

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
    
    return true;
}

int select_crawling_edge(CFDSurface& surface, CFDPolygon& quad, polygon_handle* new_ids, QuadSelectionInfo* info) {
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
    
    /*int crawl_from = -1;
    float highest = 0.0f;
    for (uint i = 0; i < 4; i++) {
        polygon_handle p = quad.edges[i].neighbor;
        
        float score = 0.0f;
        find_best_quad_edge_and_score(new_ids, surface.polygons[p.id], info[p.id], &score);
        
        if (score > highest) {
            highest = score;
            crawl_from = i;
        }
    }*/
    
    return crawl_from;
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

	for (int i = surface.polygons.length - 1; i >= 0; i--) {
		//if (edge_loops == 8) break;

		TopScore top_score = top_scores[i];
		if (new_ids[top_score.id.id].id != -1) continue;

		CFDPolygon starting_quad;
        int connecting_edge = find_best_quad_edge(new_ids, surface.polygons[top_score.id.id], infos[top_score.id.id]);
        if (!add_quad(starting_quad, polygons, new_ids, surface, top_score.id, connecting_edge)) continue;

        int start_crawl_from = select_crawling_edge(surface, starting_quad, new_ids, infos);
        
        scan_edgeloop(polygons, surface, new_ids, infos, starting_quad, start_crawl_from); //forwards
        scan_edgeloop(polygons, surface, new_ids, infos, starting_quad, start_crawl_from + 2); //backwards
        
        /*
        for (uint dir = 0; dir < 2; dir++) {
            CFDPolygon quad = starting_quad;
            int crawl_from = !start_crawl_from;
            
            while (true) {
                polygon_handle adjacent = quad.edges[!crawl_from].neighbor;
                if (adjacent.id == -1 || new_ids[adjacent.id].id != -1) break;
                int connecting_edge = find_best_quad_edge(new_ids, surface.polygons[adjacent.id], infos[adjacent.id]);
                if (!add_quad(quad, polygons, new_ids, surface, adjacent, connecting_edge)) break;

                crawl_from = select_crawling_edge(surface, quad, new_ids, infos);

                if (!scan_edgeloop(polygons, surface, new_ids, infos, quad, crawl_from)) break;
                scan_edgeloop(polygons, surface, new_ids, infos, quad, crawl_from + 2); //backwards
                
                crawl_from = !crawl_from;
            }
        }*/
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

void compute_normals(slice<CFDVertex> vertices, CFDCell& cell) {
    const ShapeDesc& shape = shapes[cell.type];
    
    for (uint i = 0; i < shape.num_faces; i++) {
        const ShapeDesc::Face& face = shape.faces[i];
        vec3 positions[4];
        uint verts = face.num_verts;
        for (uint j = 0; j < verts; j++) {
            vertex_handle vertex = cell.vertices[face.verts[j]];
            positions[j] = vertices[vertex.id].position;
        }
        
        cell.faces[i].normal = verts == 3 ? triangle_normal(positions) : quad_normal(positions);
    }
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

			vec3 mid_point = (vertex_positions[a] + vertex_positions[b]) / 2.0f; //todo use vertex indices instead

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
	}

	return {std::move(triangles), vertices};
}

struct Grid {
    bool* occupied;
	vertex_handle* vertices;
    uint width;
    uint height;
    uint depth;
    
    glm::vec3 min;
    float resolution;
    
    uint index(glm::ivec3 vec) {
        return vec.x + vec.y * width + vec.z * width * height;
    }
    
    glm::ivec3 pos(uint index) {
        int wh = width * height;
        glm::ivec3 pos;
        pos.z = index / wh;
        int rem = index - pos.z*wh;
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
    glm::ivec3(0,-1,0 ),
    glm::ivec3(-1,0 ,0 ),
    glm::ivec3(0 ,0 ,-1 ),
    glm::ivec3(1 ,0 ,0 ),
    glm::ivec3(0 ,0 ,1),
    glm::ivec3(0 ,1 ,0 )
};

uint opposing_face[6] = {
    5,
    3,
    4,
    1,
    2,
    0
};

vertex_handle add_grid_vertex(CFDVolume& volume, Grid& grid, uint x, uint y, uint z, bool* unique) {
	uint index = x + y * (grid.width+1) + z * (grid.width+1) * (grid.height+1);
	vertex_handle& result = grid.vertices[index];
	
	if (result.id == -1) {
		float resolution = grid.resolution;
		glm::vec3 min = grid.min;
		glm::vec3 pos = { min.x + x * resolution, min.y + y * resolution, min.z + z * resolution };

		result = { (int)volume.vertices.length };
		volume.vertices.append({ pos });
		*unique = true;
	}
	else {
		*unique = false;
	}

	return result;
}

void build_grid(CFDVolume& mesh, vector<vertex_handle>& boundary_verts, tvector<Boundary>& boundary, uint& extruded_vertices_offset, uint& extruded_cell_offset, float resolution, uint layers) {
	LinearRegion region(get_temporary_allocator());
	
	uint cell_count = mesh.cells.length;
    
    AABB domain_bounds;
    for (uint i = extruded_vertices_offset; i < mesh.vertices.length; i++) {
        domain_bounds.update(mesh.vertices[i].position);
    }
    
    domain_bounds.min -= resolution * layers;
    domain_bounds.max += resolution * layers;
    
    glm::vec3 size = domain_bounds.size();
    
    Grid grid;
    grid.resolution = resolution;
    grid.min = domain_bounds.min;
    grid.width = (size.x+1) / resolution;
    grid.height = (size.y+1) / resolution;
    grid.depth = (size.z+1) / resolution;
    
    /*if (grid.width  % 2 != 0) {
        grid.min.x -= 0.5*resolution;
        grid.width++;
    }
    if (grid.height % 2 != 0) {
        grid.min.y -= 0.5*resolution;
        grid.height++;
    }
    if (grid.depth  % 2 != 0) {
        grid.min.z -= 0.5*resolution;
        grid.depth++;
    }*/
    
    //grid.width = ceilf(grid.width/2.0) * 2;
    //grid.height = ceilf(grid.height/2.0) * 2;
    //grid.depth = ceilf(grid.depth/2.0) * 2;
    
    
    
    uint grid_cells = grid.width*grid.height*grid.depth;
    uint grid_cells_plus1 = (grid.width+1)*(grid.height+1)*(grid.depth+1);
    grid.occupied = TEMPORARY_ZEROED_ARRAY(bool, grid_cells);
	grid.vertices = TEMPORARY_ZEROED_ARRAY(vertex_handle, grid_cells_plus1); 
	//quite a large allocation, maybe a hashmap is more efficient, since it will be mostly empty
    
    glm::vec3 offset = glm::vec3(0.25*resolution);
    
    //RASTERIZE OUTLINE TO GRID
    for (uint cell_id = extruded_cell_offset; cell_id < cell_count; cell_id++) {
        CFDCell& cell = mesh.cells[cell_id];
        AABB aabb;
        uint verts = shapes[cell.type].num_verts;
        for (uint i = 0; i < verts; i++) {
            vec3 position = mesh.vertices[cell.vertices[i].id].position;
            aabb.update(position);
        }
        
        aabb.min -= offset;
        aabb.max += offset;
        
        //RASTERIZE AABB
        glm::ivec3 min_i = glm::ivec3(glm::floor((aabb.min - grid.min) / grid.resolution));
        glm::ivec3 max_i = glm::ivec3(glm::ceil((aabb.max - grid.min) / grid.resolution));
        
        for (uint z = min_i.z; z < max_i.z; z++) {
            for (uint y = min_i.y; y < max_i.y; y++) {
                for (uint x = min_i.x; x < max_i.x; x++) {
                    grid[glm::ivec3(x,y,z)] = true;
                }
            }
        }
    }
    
    //Flood Fill to determine what's inside and out
    //Any Cell that is visited is outside if we start at the corner
    bool* visited = TEMPORARY_ZEROED_ARRAY(bool, grid_cells);
    
    tvector<uint> stack;
    stack.append(0);
    visited[0] = true; //todo assert corner is empty
    
    while (stack.length > 0) {
        uint index = stack.pop();
        glm::ivec3 pos = grid.pos(index);
        
        for (uint i = 0; i < 6; i++) {
            glm::ivec3 p = pos + grid_offsets[i];
            if (!grid.is_valid(p)) continue;
            
            uint neighbor_index = grid.index(p);
            if (!grid.occupied[neighbor_index] && !visited[neighbor_index]) {
                visited[neighbor_index] = true;
                stack.append(neighbor_index);
            }
        }
    }
    
    cell_handle* cell_ids = TEMPORARY_ARRAY(cell_handle, grid_cells);
    
    //GENERATE OUTLINE
    bool* empty = grid.occupied;
    grid.occupied = visited;

    for (uint n = 0; n < layers; n++) {
        for (uint z = 0; z < grid.depth; z++) {
            for (uint y = 0; y < grid.height; y++) {
                for (uint x = 0; x < grid.width; x++) {
                    glm::ivec3 pos = glm::ivec3(x,y,z);
                    
                    uint index = grid.index(pos);
                    bool is_empty = grid.occupied[index];
                    bool fill = false;
					
					bool neighbor[6] = {};

                    if (is_empty) {
                        for (uint i = 0; i < 6; i++) {
                            glm::ivec3 p = pos + grid_offsets[i];
							if (grid.is_valid(p)) {
								bool boundary = !grid[p];
								neighbor[i] = boundary;
								fill |= boundary;
							}
                        }
                    }
                    
                    if (fill) {
                        empty[index] = false;
        
                        CFDCell cell;
                        cell.type = CFDCell::HEXAHEDRON;
                        cell_handle curr = {(int)mesh.cells.length};
                        
                        //todo: counter-clockwise
						bool is_unique[8];
						cell.vertices[0] = add_grid_vertex(mesh, grid, x  , y, z+1, is_unique+0);
						cell.vertices[1] = add_grid_vertex(mesh, grid, x+1, y, z+1, is_unique+1);
						cell.vertices[2] = add_grid_vertex(mesh, grid, x+1, y, z  , is_unique+2);
						cell.vertices[3] = add_grid_vertex(mesh, grid, x, y, z, is_unique + 3);
						
						cell.vertices[4] = add_grid_vertex(mesh, grid, x  , y+1, z+1, is_unique+4);
						cell.vertices[5] = add_grid_vertex(mesh, grid, x+1, y+1, z+1, is_unique+5);
						cell.vertices[6] = add_grid_vertex(mesh, grid, x+1, y+1, z  , is_unique+6);
						cell.vertices[7] = add_grid_vertex(mesh, grid, x, y + 1, z, is_unique + 7);

						for (uint i = 0; i < 6; i++) {
                            glm::vec3 p = pos + grid_offsets[i];
                            if (grid.is_valid(p)) {
                                cell_handle neighbor = cell_ids[grid.index(p)];
                                if (neighbor.id != -1) {
                                    cell.faces[i].neighbor = neighbor; //mantain doubly linked connections
                                    mesh[neighbor].faces[opposing_face[i]].neighbor = curr;
                                }
                            } else {
                                cell.faces[i].neighbor.id = -1;
                            }
						}

						if (n == 0) {
							for (uint i = 0; i < 6; i++) {
								if (!neighbor[i]) continue;
								cell.faces[i].neighbor.id = -1;

								Boundary face;
								face.cell = { (int)mesh.cells.length };
								for (uint j = 0; j < 4; j++) {
									uint index = hexahedron_shape.faces[i].verts[j];
									vertex_handle v = cell.vertices[index];
									face.vertices[j] = v;
									if (is_unique[index]) {
										boundary_verts.append(v);
										is_unique[index] = false;
									}
								}

								boundary.append(face);
							}

                        }

                        cell_ids[index] = curr;
						mesh.cells.append(cell);
                    } else {
                        empty[index] = is_empty;
                    }
                }
            }
        }
        
        std::swap(grid.occupied, empty);
    }
}

float frand() {
	return (float)rand() / INT_MAX * 2.0 - 1.0;
}

#include <random>

#include "core/profiler.h"

CFDVolume generate_mesh(World& world, CFDMeshError& err) {
	auto some_mesh = world.first<Transform, CFDMesh>();
	auto some_domain = world.first<Transform, CFDDomain>();

	CFDVolume result;
    
    Profile profile("Generate mesh");

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
		
        
		if (false) {
			tvector<Boundary> boundary;
			tvector<vertex_handle> boundary_verts;

			for (uint i = 0; i < 1000; i++) {
				vec3 pos;
				pos.x = (float)rand() / INT_MAX * 10;
				pos.y = (float)rand() / INT_MAX * 10;
				pos.z = (float)rand() / INT_MAX * 10;

				boundary_verts.append({ (int)result.vertices.length });
				result.vertices.append({ pos });
			}

			build_deluanay(result, boundary_verts, boundary);
			return result;
		}
	
        //Advancing Front
        Delaunay delaunay(result, domain_bounds);
        vector<vertex_handle> boundary_verts;
        
        {
            CFDSurface surface = surface_from_mesh(result.vertices, model_m, model->meshes[0]);
            DelaunayFront front(delaunay, result, surface);
            front.generate_n_layers(n, initial, domain.contour_thickness_expontent);
        }

		tvector<Boundary> boundary;

		//tvector<vertex_handle> boundary_verts_a;
		//tvector<vertex_handle> boundary_verts_b;

		boundary_verts.reserve(result.vertices.length - extruded_vertice_watermark);
		for (int i = extruded_vertice_watermark; i < result.vertices.length; i++) {
			boundary_verts.append({ i });
		}

		boundary.reserve(result.cells.length - extruded_cells_watermark);
		for (int i = extruded_cells_watermark; i < result.cells.length; i++) {
			//assume top face is unconnected
			Boundary face;
			face.cell = { i };
			for (uint j = 0; j < 4; j++) {
				face.vertices[j] = result.cells[i].vertices[j + 4];
			}

			boundary.append(face);
		}

		uint seed = 0;

		//result.cells.clear();
		uint prev = result.cells.length;
		//boundary_verts.clear();
        build_grid(result, boundary_verts, boundary, extruded_vertice_watermark, extruded_cells_watermark, domain.grid_resolution, domain.grid_layers);		
		
		//std::shuffle(boundary_verts.begin(), boundary_verts.end(), std::default_random_engine(seed));

		/*tvector<vertex_handle> boundary_verts;
		uint len = min(boundary_verts_a.length, boundary_verts_b.length);
		for (uint i = 0; i < len; i++) {
			boundary_verts.append(boundary_verts_a[i]);
			boundary_verts.append(boundary_verts_b[i]);
		}
		for (uint i = len; i < boundary_verts_a.length; i++) {
			boundary_verts.append(boundary_verts_a[i]);
		}
		for (uint i = len; i < boundary_verts_b.length; i++) {
			boundary_verts.append(boundary_verts_b[i]);
		}*/

		//boundary_verts.length = 300;

		prev = result.cells.length;
		//advancing_front_triangulation(result, extruded_vertice_watermark, extruded_cells_watermark, domain_bounds);
		
		//printf("================\nOccupied %ull\n", boundary_verts.allocator->occupied);

		//result.cells.clear();
		//build_deluanay(result, boundary_verts, boundary);


		printf("Deluanay generated %i cells\n", result.cells.length - prev);
		/*Front front(result.vertices, result.cells, domain_bounds);

		for (uint i = 0; i < domain.tetrahedron_layers; i++) {
			advancing_front_triangulation(result, front, extruded_vertice_watermark, extruded_cells_watermark, 0.1);
		}*/
	}
	else {
		err.type = CFDMeshError::NoMeshOrDomain;
	}
    
    profile.end();
    printf("==== Generated mesh in %f ms, verts: %i, cells: %i\n\n\n", profile.duration() * 1000, result.vertices.length, result.cells.length);

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
