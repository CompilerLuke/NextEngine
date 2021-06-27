#include "mesh/surface_tet_mesh.h"
#include "visualization/debug_renderer.h"

SurfaceTriMesh::SurfaceTriMesh() {}

SurfaceTriMesh::~SurfaceTriMesh() {
	free(edges);
	free(indices);
}

SurfaceTriMesh::SurfaceTriMesh(SurfaceTriMesh&& other) {
	tri_capacity = other.tri_capacity;
	tri_count = other.tri_count;
	positions = std::move(other.positions);
	edges = other.edges;
	indices = other.indices;
    flags = other.flags;
	aabb = other.aabb;

	other.edges = nullptr;
	other.indices = nullptr;
}

SurfaceTriMesh::SurfaceTriMesh(const SurfaceTriMesh& other) {
	tri_capacity = other.tri_capacity;
	tri_count = other.tri_count;
	positions = other.positions;

	uint n = tri_count * 3;
	edges = new edge_handle[n];
	indices = new vertex_handle[n];
	flags = new char[tri_count];

	memcpy_t(edges, other.edges, n);
	memcpy_t(indices, other.indices, n);
	memcpy_t(flags, other.flags, tri_count);

	aabb = other.aabb;
}

void SurfaceTriMesh::operator=(SurfaceTriMesh&& other) {
	//todo free previous
	
	tri_capacity = other.tri_capacity;
	tri_count = other.tri_count;
	positions = std::move(other.positions);
	edges = other.edges;
	indices = other.indices;
    flags = other.flags;
	aabb = other.aabb;

	other.edges = nullptr;
	other.indices = nullptr;
    other.flags = nullptr;
}

void SurfaceTriMesh::reserve_tris(uint count) {
	if (count < tri_capacity) return;

	tri_capacity = count;
	edges = realloc_t(edges, count*3);
	indices = realloc_t(indices, count*3);
	flags = realloc_t(flags, count);
}

//todo this function is incorrect
void SurfaceTriMesh::resize_tris(uint count) {
	reserve_tris(count);
	for (uint i = 0; i < count*3; i++) {
		indices[i] = {};
		edges[i] = {};
	}
    for (uint i = 0; i < count; i++) {
        flags[i] = {};
    }
	tri_count = count;
}

#define DEBUG_TRACE_TRISEARCH

void SurfaceTriMesh::dealloc_tri(tri_handle tri) {
    flags[tri / 3] |= TRI_DELETED;
    free_tris.append(tri);
}

tri_handle SurfaceTriMesh::project(CFDDebugRenderer& debug, tri_handle start, vec3* pos_ptr, float* pos_disp) {
    vec3 pos = *pos_ptr;
    
    tri_handle current = start;
    tri_handle last = 0;
    uint counter = 5;
    
    const uint n = 100;
#ifdef DEBUG_TRACE_TRISEARCH
    array<n, vec3> path;
    path.append(triangle_center(current));
#endif

    while (true) {
        vec3 v[3];
        triangle_verts(current, v);
        vec3 normal = triangle_normal(v);
        vec3 center = (v[0]+v[1]+v[2])/3.0f;
        
        float disp = dot(pos - center, normal);
        vec3 project = pos - disp*normal;

        vec3 e0 = v[1]-v[0];
        vec3 e1 = v[2]-v[1];
        vec3 e2 = v[0]-v[2];
    
        vec3 c0 = project - v[0];
        vec3 c1 = project - v[1];
        vec3 c2 = project - v[2];
        
        float d0 = dot(normal, cross(e0, c0));
        float d1 = dot(normal, cross(e1, c1));
        float d2 = dot(normal, cross(e2, c2));

        const float threshold = 0.01;
    
        bool inside = d0 > -threshold && d1 > -threshold && d2 > -threshold;
        
        if (inside) {
            *pos_ptr = project;
            if (pos_disp) *pos_disp = disp;
            return current;
        }
        
        float min_length = FLT_MAX; // sq(project - center);
        tri_handle closest_neighbor = 0;
        
        for (uint j = 0; j < 3; j++) {
            tri_handle tri = neighbor(current, j);
            if (tri == last) continue;
            
            vec3 v0 = v[j];
            vec3 v1 = v[(j+1)%3];
            vec3 e = v0-v1;
            vec3 c = (v0+v1)/2.0f;
            
            //Plane test with edge
            vec3 b = normalize(cross(normal, e));
            float distance_to_edge = dot(project - c, b);
            
            //sq(pos - center);
            if (distance_to_edge > 0 && distance_to_edge < min_length) {
                closest_neighbor = tri;
                min_length = distance_to_edge;
            }
        }

        if (false && !closest_neighbor) {
            tri_handle options[2];
            uint count = 0;
            for (uint i = 0; i < 3; i++) {
                tri_handle tri = neighbor(current, i);
                if (tri == last) continue;

                options[count++] = tri;
            }
            closest_neighbor = options[rand() % count];
        }

        if (closest_neighbor) {
            last = current;
            current = closest_neighbor;
            
#ifdef DEBUG_TRACE_TRISEARCH
            path.append(center);
#endif
            
            if (++counter >= n) {
#ifdef DEBUG_TRACE_TRISEARCH
                for (uint i = 0; i < path.length-1; i++) {
                    draw_line(debug, path[i], path[i+1], vec4(0,0,1,1));
                }
                draw_line(debug, path[path.length-1], pos, vec4(0,1,0,1));

                draw_point(debug, path[0], vec4(0,1,0,1));
                draw_point(debug, pos, vec4(0,0,1,1));
                
                suspend_execution(debug);
#endif
                
                
                return 0;
            }
        } else {
            draw_line(debug, center, center + normal, vec4(0, 0, 1, 1));
            draw_point(debug, pos, vec4(0, 0, 1, 1));

            suspend_execution(debug);

            break;
        }
    }
       
    return 0;
}
