#include "mesh/surface_tet_mesh.h"

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

	memcpy_t(edges, other.edges, n);
	memcpy_t(indices, other.indices, n);

	aabb = other.aabb;
}

void SurfaceTriMesh::operator=(SurfaceTriMesh&& other) {
	//todo free previous
	
	tri_capacity = other.tri_capacity;
	tri_count = other.tri_count;
	positions = std::move(other.positions);
	edges = other.edges;
	indices = other.indices;
	aabb = other.aabb;

	other.edges = nullptr;
	other.indices = nullptr;
}

void SurfaceTriMesh::reserve_tris(uint count) {
	if (count < tri_capacity) return;

	tri_capacity = count;
	edges = realloc_t(edges, count*3);
	indices = realloc_t(indices, count*3);
}

void SurfaceTriMesh::resize_tris(uint count) {
	reserve_tris(count);
	for (uint i = 0; i < count*3; i++) {
		indices[i] = {};
		edges[i] = {};
	}
	tri_count = count;
}


tri_handle SurfaceTriMesh::project(tri_handle start, vec3* pos_ptr, float* pos_disp) {
    vec3 pos = *pos_ptr;
    
    tri_handle current = start;
    tri_handle last = 0;
    uint counter = 5;
    
    const uint n = 20;
#ifdef DEBUG_TRACE_TRISEARCH
    array<n, vec3> path;
    path.append(mesh.triangle_center(current));
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
    
        bool inside = dot(normal, cross(e0, c0)) > 0 &&
                      dot(normal, cross(e1, c1)) > 0 &&
                      dot(normal, cross(e2, c2)) > 0;
        
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
#endif
                //suspend_execution(debug);
                
                return 0;
            }
        } else {
            break;
        }
    }
       
    return 0;
}
