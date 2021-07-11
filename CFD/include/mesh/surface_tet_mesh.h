#pragma once

#include "core/container/vector.h"
#include "core/math/vec3.h"
#include "core/math/vec4.h"
#include "core/math/aabb.h"
#include "mesh.h"
#include "cfd_core.h"

#include "visualization/debug_renderer.h"

struct stable_edge_handle {
	uint id = 0;
};

namespace std {
	template <>
	struct hash<stable_edge_handle>
	{
		inline std::size_t operator()(const stable_edge_handle& k) const
		{
			return k.id;
		}
	};

}

inline u64 hash_func(stable_edge_handle a) { return a.id; }
inline bool operator==(stable_edge_handle a, stable_edge_handle b) { return a.id == b.id; }
inline bool operator!=(stable_edge_handle a, stable_edge_handle b) { return a.id != b.id; }


struct SurfaceTriMeshIt {
	uint i;

	inline void operator++() { i += 3; }
	inline tri_handle operator*() { return i; }
	inline bool operator==(SurfaceTriMeshIt& it) { return i == it.i; }
	inline bool operator!=(SurfaceTriMeshIt& it) { return i != it.i; }
};

constexpr char TRI_DELETED = 1 << 0;

constexpr char BOUNDARY_EDGE = 1 << 0;

struct CFDDebugRenderer;

struct SurfaceTriMesh;

void draw_mesh(CFDDebugRenderer& debug, SurfaceTriMesh&);
void draw_triangle(CFDDebugRenderer& debug, SurfaceTriMesh&, tri_handle, vec4);
void draw_edge(CFDDebugRenderer& debug, SurfaceTriMesh&, edge_handle, vec4);

struct SurfaceTriMesh {
	CFDDebugRenderer* debug = nullptr;

	uint tri_capacity = 0;
	uint tri_count = 0;
	vector<vec3> positions;
	vector<tri_handle> free_tris;
	edge_handle* edges = nullptr;
	vertex_handle* indices = nullptr;
	char* flags = nullptr;
	char* edge_flags; // = nullptr;
	stable_edge_handle* edge_to_stable = nullptr;
	vector<edge_handle> stable_to_edge;
	stable_edge_handle free_stable_edge;
	
	AABB aabb;

	void copy(const SurfaceTriMesh&);
	void move(SurfaceTriMesh&&);

	SurfaceTriMesh();
	SurfaceTriMesh(const SurfaceTriMesh&);
	SurfaceTriMesh(SurfaceTriMesh&&);
	void operator=(SurfaceTriMesh&&);
	~SurfaceTriMesh();

	//Stable edges
	void mark_new_edge(edge_handle, bool both = true);
	void remove_edge(edge_handle);
	void map_stable_edge(edge_handle, stable_edge_handle);

	edge_handle get_edge(stable_edge_handle);
	tri_handle get_tri(stable_edge_handle);
	stable_edge_handle get_stable(edge_handle);
	stable_edge_handle get_opp(stable_edge_handle);
	edge_handle get_opp_edge(stable_edge_handle);


	//Mesh operations
	void link_edge(edge_handle edge, edge_handle neigh);
	edge_handle flip_edge(edge_handle edge0, bool force = false);
	bool flip_bad_edge(edge_handle edge0);
	stable_edge_handle split_edge(edge_handle edge0, vec3 pos);
	stable_edge_handle split_and_flip(edge_handle edge0, vec3 pos);
	void flip_bad_edges(tri_handle start, bool smooth = true);
	void move_vert(edge_handle dir_edge, vec3 new_pos);

	//Smoothing
	void smooth_mesh(uint n = 1);

	//Memory
	void reserve_tris(uint count);
	void resize_tris(uint count);
    
	void dealloc_tri(tri_handle tri);


	inline tri_handle alloc_tri(uint count = 1) {
		if (free_tris.length > 0) {
			tri_handle tri = free_tris.pop();
			flags[tri / 3] = 0;
			return tri;
		}
		
		if (tri_count+count > tri_capacity) {
			reserve_tris(max(tri_count * 2, tri_count + count));
		}
		tri_handle base = tri_count * 3;
		flags[tri_count] = 0;
		tri_count += count;
		return base;
	}

	//Miscelaneous
	inline void mark_boundary(stable_edge_handle edge) {
		edge_flags[edge.id] |= BOUNDARY_EDGE;
		edge_flags[get_opp(edge).id] |= BOUNDARY_EDGE;
	}
    
	//Queries
	tri_handle project(CFDDebugRenderer& debug, tri_handle start, vec3* pos, float* disp);

	inline vec3 position(tri_handle tri, uint j = 0) {
		return positions[indices[tri + j].id];
	}

	inline tri_handle neighbor(tri_handle tri, uint j) { 
		return TRI(edges[tri + j]);  
	}

	inline edge_handle neighbor_edge(tri_handle tri, uint j) { 
		return edges[tri + j] % 3;
	}

	inline void triangle_verts(tri_handle tri, vertex_handle v[3]) {
		for (uint i = 0; i < 3; i++) v[i] = indices[tri+i];
	}

	inline void triangle_verts(tri_handle tri, vec3 v[3]) {
		for (uint i = 0; i < 3; i++) v[i] = position(tri, i);
	}

	inline void edge_verts(edge_handle edge, vertex_handle* v0, vertex_handle* v1) {
		*v0 = indices[edge];
		*v1 = indices[TRI(edge) + (TRI_EDGE(edge) + 1) % 3];
	}

	inline void edge_verts(edge_handle edge, vec3* p0, vec3* p1) {
		*p0 = position(edge, 0);
		*p1 = position(TRI(edge), (TRI_EDGE(edge) + 1) % 3);
	}
    
    inline vec3 edge_center(edge_handle edge) {
        vec3 v0, v1;
        edge_verts(edge, &v0, &v1);
        return (v0 + v1) / 2.0f;
    }
    
    inline vec3 triangle_center(tri_handle tri) {
        vec3 v[3];
        triangle_verts(tri, v);
        return (v[0] + v[1] + v[2]) / 3.0f;
    }

	inline vec3 triangle_normal(tri_handle tri) {
		vec3 v[3];
		triangle_verts(tri, v);
		return ::triangle_normal(v);
	}

	inline bool is_deallocated(tri_handle tri) {
		return flags[tri/3] & TRI_DELETED;
	}

	inline bool is_boundary(edge_handle edge) {
		stable_edge_handle stable = get_stable(edge);
		return edge_flags[stable.id] & BOUNDARY_EDGE;
	}

	inline void diamond_verts(edge_handle edge0, vec3 p[4]) {
		edge_handle edge1 = edges[edge0];

		for (uint i = 0; i < 3; i++) {
			p[i] = position(TRI_NEXT_EDGE(edge0, i + 1));
		}
		p[3] = position(TRI_NEXT_EDGE(edge1, 2));
	}

	inline SurfaceTriMeshIt begin() { return { 3 }; }
	inline SurfaceTriMeshIt end() { return {tri_count * 3}; }

	array<100, edge_handle> ccw(edge_handle edge) {
		vertex_handle vert = indices[edge];
		
		vec3 start_normal;

		const uint n = 100;
		array<n, edge_handle> result;
		result.append(edge);

		bool debug_ccw = false;

		//todo could be done more efficiently
		//if you start on the left edge of the triangle
		//you can always advance by 2
		uint count = 0;
		edge_handle e1 = edge;
		while (true) {
			count++;
			e1 = TRI_NEXT_EDGE(e1);

			if (debug_ccw) {
				//clear_debug_stack(*debug);
				draw_triangle(*debug, *this, TRI(e1), RED_DEBUG_COLOR);
				draw_edge(*debug, *this, e1, BLUE_DEBUG_COLOR);
				draw_edge(*debug, *this, edge, GREEN_DEBUG_COLOR);
				draw_point(*debug, position(edge), GREEN_DEBUG_COLOR);
				suspend_execution(*debug);
			}

			vertex_handle v0, v1;
			edge_verts(e1, &v0, &v1);

			edge_handle dir_edge = 0;
			if (v0 == vert) dir_edge = e1;
			else if (v1 == vert) dir_edge = edges[e1];
			else continue;

			if (count < n) result.append(dir_edge);
			
			e1 = edges[e1];
			if (e1 == edge || e1 == edges[edge]) break;
			if (count > 30) {
				debug_ccw = true;
			}
		}

		return result;
	}
};