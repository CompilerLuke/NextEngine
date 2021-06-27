#pragma once

#include "core/container/vector.h"
#include "core/math/vec3.h"
#include "core/math/aabb.h"
#include "mesh.h"
#include "cfd_core.h"

struct SurfaceTriMeshIt {
	uint i;

	inline void operator++() { i += 3; }
	inline tri_handle operator*() { return i; }
	inline bool operator==(SurfaceTriMeshIt& it) { return i == it.i; }
	inline bool operator!=(SurfaceTriMeshIt& it) { return i != it.i; }
};

constexpr char TRI_DELETED = 1 << 0;

struct SurfaceTriMesh {
	uint tri_capacity = 0;
	uint tri_count = 0;
	vector<vec3> positions;
	vector<tri_handle> free_tris;
	edge_handle* edges = nullptr;
	vertex_handle* indices = nullptr;
	char* flags = nullptr;
	AABB aabb;

	SurfaceTriMesh();
	SurfaceTriMesh(const SurfaceTriMesh&);
	SurfaceTriMesh(SurfaceTriMesh&&);
	void operator=(SurfaceTriMesh&&);
	~SurfaceTriMesh();

	void reserve_tris(uint count);
	void resize_tris(uint count);
    
	void dealloc_tri(tri_handle tri);

    tri_handle project(CFDDebugRenderer& debug, tri_handle start, vec3* pos, float* disp);

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

	inline bool is_deallocated(tri_handle tri) {
		return flags[tri/3] & TRI_DELETED;
	}

	inline SurfaceTriMeshIt begin() { return { 3 }; }
	inline SurfaceTriMeshIt end() { return {tri_count * 3}; }
};
