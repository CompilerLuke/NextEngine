#include <glm/vec3.hpp>
#include "core/core.h"
#include "mesh_generation/delaunay_advancing_front.h"
#include "mesh_generation/delaunay.h"
#include "mesh_generation/surface_point_placement.h"
#include "mesh_generation/point_octotree.h"
#include "mesh/surface_tet_mesh.h"
#include "mesh/feature_edges.h"

#undef max
#undef min

#include "cfd_ids.h"
#include "mesh.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include "cfd_components.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"

#include <algorithm>
#include <queue>
#include <set>


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

#include <random>

#include "core/profiler.h"
#include "mesh_generation/cross_field.h"
#include "mesh/edge_graph.h"
#include "visualization/debug_renderer.h"
#include "core/math/aabb.h"

#include <set>

struct HexcoreCell {
    u64 morton;
    uint depth; //todo could compute this implicitly, but probably more error prone
    union {
        HexcoreCell* parent;
        HexcoreCell* next_free;
    };
    HexcoreCell* children;
};

const uint CHUNK_SIZE = mb(50);

HexcoreCell* alloc_8_hexcore_cell(HexcoreCell** pool) {
    if (!*pool) {
        *pool = (HexcoreCell*)malloc(CHUNK_SIZE);
        uint n = CHUNK_SIZE/(8*sizeof(HexcoreCell));
        for (uint i = 0; i < n-1; i++) {
            (*pool)[i*8].next_free = *pool + (i+1)*8;
        }
        (*pool)[(n-1)*8].next_free = nullptr;
    }
    
    HexcoreCell* current = *pool;
    *pool = current->next_free;
    return current;
}

void hexcore_to_mesh(CFDVolume& volume, HexcoreCell* root, const AABB& aabb) {
    struct Data {
        AABB aabb;
        HexcoreCell* cells;
    };
    
    tvector<Data> stack;
    stack.append({aabb, root->children});
    
    while (stack.length > 0) {
        Data data = stack.pop();
        
        AABB child_aabbs[8];
        subdivide_aabb(data.aabb, child_aabbs);
        
        for (uint i = 0; i < 8; i++) {
            if (!data.cells[i].children) {
                uint subdivision = 2;
                vec3 dx = child_aabbs[i].size() / subdivision;
                
                for (uint x = 0; x < subdivision; x++) {
                    for (uint y = 0; y < subdivision; y++) {
                        for (uint z = 0; z < subdivision; z++) {
                            AABB aabb;
                            aabb.min = child_aabbs[i].min + vec3(x,y,z)*dx;
                            aabb.max = aabb.min + dx;
                            
                            glm::vec3 points[8];
                            aabb.to_verts(points);
                            
                            CFDCell cell{CFDCell::HEXAHEDRON};
                            
                            for (uint j = 0; j < 8; j++) {
                                cell.vertices[j] = {(int)volume.vertices.length};
                                volume.vertices.append({points[j]});
                            }
                            
                            volume.cells.append(cell);
                        }
                    }
                }
                
                
            } else {
                stack.append({child_aabbs[i], data.cells[i].children});
            }
        }
    }
}

#define MORTON_MASK(depth, i) ((u64)(i) << (depth-1)*3)
#define MORTON_AXIS(depth, k) ((u64)(1) << ((depth-1)*3+k))

//todo clean up leak
HexcoreCell* find_cell(HexcoreCell* start, u64 morton, uint f) {
    if (morton == UINT64_MAX) return nullptr;
    
    HexcoreCell* current = start;
    while (current && current->depth >= f) { //(current->morton & morton) != current->morton ) {
        current = current->parent;
    }
    if (!current) return nullptr;
    
    while (current->children && current->depth < start->depth+1) {
        uint index = (morton >> 3*current->depth) & (1<<3)-1;
        current = current->children + index;
    }
    
    if (current->children) return nullptr;
    else return current;
}

// Assumes little endian
void printBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;
    
    for (i = size-1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}

u64 neighbor_code(u64 code, uint depth, uint k, uint* f) {
    u64 mask = MORTON_AXIS(depth, k);
    bool b = code & mask;
    u64 result = code ^ mask;
    for (uint i = depth-1; i > 0; i--) {
        u64 mask = MORTON_AXIS(i, k);
        result ^= mask;
        if (b != bool(code & mask)) {
            *f = i;
            return result;
        }
    }
    
    return UINT64_MAX;
}

void build_hexcore(HexcoreCell** pool, HexcoreCell* root, PointOctotree& octo) {
    struct Data {
        HexcoreCell* parent;
        HexcoreCell* cells;
        PointOctotree::Payload* payload;
    };
    
    root->children = alloc_8_hexcore_cell(pool);
                       
    tvector<Data> stack;
    stack.append({root, root->children, octo.root.p});
    
    while (stack.length > 0) {
        Data data = stack.pop();
        HexcoreCell* cells = data.cells;
        u64 morton = data.parent->morton;
        uint depth = data.parent->depth + 1;
        
        for (uint i = 0; i < 8; i++) {
            auto& subdivision = data.payload->children[i];
            u64 child_morton = morton | MORTON_MASK(depth, i);
            cells[i].morton = child_morton;
            cells[i].parent = data.parent;
            cells[i].depth = depth;
            
            if (subdivision.count <= PointOctotree::MAX_PER_CELL) {
                cells[i].children = nullptr;
            } else {
                cells[i].children = alloc_8_hexcore_cell(pool);
                stack.append({ cells+i, cells[i].children, subdivision.p });
            }
        }
    }
}

struct HexRefinementQueue {
    vector<HexcoreCell*> refine;
    std::set<u64> morton_codes;
};

void refine_if_needed(HexRefinementQueue& queue, uint depth, HexcoreCell* neighbor) {
    if (!neighbor) return;
    
    if (queue.morton_codes.find(neighbor->morton) != queue.morton_codes.end()) return;
    if (neighbor->depth >= depth-1) return;
    
    //printf("Refined!\n");
        
    queue.refine.append(neighbor);
    queue.morton_codes.insert(neighbor->morton);
}

void refine_children_if_needed(HexRefinementQueue& queue, HexcoreCell* children, uint n) {
    for (uint i = 0; i < n; i++) {
        HexcoreCell& cell = children[i];
        uint depth = cell.depth;
        u64 morton = cell.morton;
        
        for (uint k = 0; k < 3; k++) {
            uint f;
            
            //HexcoreCell* sibling = find_cell(cell.parent, morton ^ MORTON_AXIS(depth, k));
            HexcoreCell* neighbor = find_cell(cell.parent, neighbor_code(morton, depth, k, &f), f);
            
            //refine_if_needed(queue, depth, sibling);
            refine_if_needed(queue, depth, neighbor);
        }
    }
}

void balance_hexcore(HexcoreCell* root, HexcoreCell** pool) {
    struct Data {
        HexcoreCell* cells;
    };
    
    HexRefinementQueue queue;
    
    tvector<Data> stack;
    
    uint count = 0;
    while (count++ < 10) {
        stack.append({root->children});
        
        while (stack.length > 0) {
            Data data = stack.pop();
            
            for (uint i = 0; i < 8; i++) {
                HexcoreCell& cell = data.cells[i];
                
                if (cell.children) {
                    stack.append({cell.children});
                    continue;
                }
                
                refine_children_if_needed(queue, &cell, 1);
            }
        }
        
        if (queue.refine.length == 0) break;
        
        for (HexcoreCell* cell : queue.refine) {
            cell->children = alloc_8_hexcore_cell(pool);
            
            for (uint i = 0; i < 8; i++) {
                cell->children[i].parent = cell;
                cell->children[i].depth = cell->depth + 1;
                cell->children[i].morton = cell->morton | MORTON_MASK(cell->depth+1, i);
                cell->children[i].children = 0;
            }
        }
        
        queue.refine.clear();
        queue.morton_codes.clear();
    }
}

void hexcore(PointOctotree& octo, CFDVolume& volume, CFDDebugRenderer& debug) {
    HexcoreCell* pool = nullptr;
    HexcoreCell root = {};
    build_hexcore(&pool, &root, octo);
    balance_hexcore(&root, &pool);
    hexcore_to_mesh(volume, &root, octo.root.aabb);
}

CFDVolume generate_mesh(World& world, InputMeshRegistry& registry, CFDMeshError& err, CFDDebugRenderer& debug) {
	auto some_mesh = world.first<Transform, CFDMesh>();
	auto some_domain = world.first<Transform, CFDDomain>();

	CFDVolume result;
    
    Profile profile("Generate mesh");
    
    clear_debug_stack(debug);

	if (some_domain && some_mesh) {
		auto [mesh_entity, mesh_trans, mesh] = *some_mesh;
		auto [domain_entity, domain_trans, domain] = *some_domain;

		AABB domain_bounds;
		domain_bounds.min = domain_trans.position - domain.size;
		domain_bounds.max = domain_trans.position + domain.size;

		InputModel& model = registry.get_model(mesh.model);
		InputModelBVH& bvh = registry.get_model_bvh(mesh.model);
		glm::mat4 model_m = compute_model_matrix(mesh_trans);

		AABB mesh_bounds = model.aabb.apply(model_m);

		if (!mesh_bounds.inside(domain_bounds)) {
			err.type = CFDMeshError::MeshOutsideDomain;
			err.id = mesh_entity.id;
			return result;
		}

		float initial = domain.contour_initial_thickness;
		float a = domain.contour_thickness_expontent;
		float n = domain.contour_layers;
        
		SurfaceTriMesh& surface = model.surface[0]; //todo handle matrix transformation
		
        EdgeGraph edge_graph = build_edge_graph(surface);
        
        tvector<float> vert_curvatures = curvature_at_verts(surface, edge_graph, debug);
        tvector<FeatureCurve> features = identify_feature_edges(surface, edge_graph, domain.feature_angle, domain.min_feature_quality, debug);
        
        tvector<edge_handle> feature_edges;
        for (FeatureCurve curve : features) {
            feature_edges += curve.edges;
        }
		
        vector<vec3> points;
        
		SurfaceCrossField cross_field(surface, debug, feature_edges);
		cross_field.propagate();

		PointOctotree octotree(points, domain_bounds);
		SurfacePointPlacement surface_point_placement(surface, cross_field, octotree, vert_curvatures);
        
        surface_point_placement.propagate(features, debug);
        
        hexcore(octotree, result, debug);
        

        //Delaunay* delaunay = make_Delaunay(result, domain_bounds, debug);
        //generate_n_layers(*delaunay, surface, n, initial, domain.contour_thickness_expontent, domain.grid_resolution, domain.grid_layers, domain.quad_quality);
        //destroy_Delaunay(delaunay);
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
