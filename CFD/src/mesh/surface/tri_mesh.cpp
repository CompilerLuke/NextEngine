#include "mesh/surface_tet_mesh.h"
#include "visualization/debug_renderer.h"

SurfaceTriMesh::SurfaceTriMesh() {}

SurfaceTriMesh::~SurfaceTriMesh() {
    if (tri_count > 0) {
        free(edges);
        free(indices);
        free(flags);
        free(edge_to_stable);
        free(edge_flags);
    }
}

void SurfaceTriMesh::move(SurfaceTriMesh&& other) {
    tri_capacity = other.tri_capacity;
	tri_count = other.tri_count;
	positions = std::move(other.positions);
	edges = other.edges;
	indices = other.indices;
    flags = other.flags;
	aabb = other.aabb;
    stable_to_edge = std::move(other.stable_to_edge);
    edge_to_stable = other.edge_to_stable;
    edge_flags = std::move(other.edge_flags);
    free_tris = std::move(other.free_tris);

	other.edges = nullptr;
	other.indices = nullptr;
    other.edge_to_stable = nullptr;
    other.edge_flags = nullptr;
    other.tri_count = 0;
}

void SurfaceTriMesh::copy(const SurfaceTriMesh& other) {
    tri_capacity = other.tri_capacity;
	tri_count = other.tri_count;
	positions = other.positions;
    stable_to_edge = other.stable_to_edge;
    free_tris = std::move(free_tris);

	uint n = tri_capacity * 3;
	edges = new edge_handle[n];
	indices = new vertex_handle[n];
	flags = new char[tri_capacity];
    edge_to_stable = new stable_edge_handle[n];
    edge_flags = new char[other.stable_to_edge.length](); // (char*)calloc(1, other.stable_to_edge.length);

	memcpy_t(edges, other.edges, n);
	memcpy_t(indices, other.indices, n);    
    memcpy_t(edge_to_stable, other.edge_to_stable, n);
	memcpy_t(flags, other.flags, tri_count);
	//if (other.edge_flags) memcpy_t(edge_flags, other.edge_flags, other.stable_to_edge.length);

	aabb = other.aabb;
    free_stable_edge = other.free_stable_edge;
}

SurfaceTriMesh::SurfaceTriMesh(SurfaceTriMesh&& other) {
    move(std::move(other));
}

void SurfaceTriMesh::operator=(SurfaceTriMesh&& other) {
    this->~SurfaceTriMesh();
    move(std::move(other));
}


SurfaceTriMesh::SurfaceTriMesh(const SurfaceTriMesh& other) {
    copy(other);
}

void SurfaceTriMesh::reserve_tris(uint count) {
	if (count < tri_capacity) return;

	tri_capacity = count;
	edges = realloc_t(edges, count*3);
	indices = realloc_t(indices, count*3);
	flags = realloc_t(flags, count);
	edge_to_stable = realloc_t(edge_to_stable, count*3);
}

//todo this function is incorrect
void SurfaceTriMesh::resize_tris(uint count) {
	reserve_tris(count);
	for (uint i = 0; i < count*3; i++) {
		indices[i] = {};
		edges[i] = {};
        edge_to_stable[i] = {};
	}
    for (uint i = 0; i < count; i++) {
        flags[i] = {};
    }
	tri_count = count;
}

//#define DEBUG_TRACE_TRISEARCH

void SurfaceTriMesh::dealloc_tri(tri_handle tri) {
    flags[tri / 3] |= TRI_DELETED;
    free_tris.append(TRI(tri));
}

tri_handle SurfaceTriMesh::project(CFDDebugRenderer& debug, tri_handle start, vec3* pos_ptr, float* pos_disp) {
    start = TRI(start);
    
    vec3 pos = *pos_ptr;
    
    tri_handle current = start;
    tri_handle last = 0;
    uint counter = 5;
    
    const uint n = 20;
#ifdef DEBUG_TRACE_TRISEARCH
    array<n, vec3> path;
    path.append(triangle_center(current));
#endif

    while (true) {
        vec3 v[3];
        triangle_verts(current, v);
        vec3 normal = ::triangle_normal(v);
        vec3 center = (v[0]+v[1]+v[2])/3.0f;
        
        float disp = dot(pos - center, normal);
        vec3 project_orig = pos - disp*normal;
        
        const float threshold = 0.001;

        vec3 pc = normalize(center - project_orig);
        vec3 project = project_orig + pc * threshold;

        vec3 e0 = v[1]-v[0];
        vec3 e1 = v[2]-v[1];
        vec3 e2 = v[0]-v[2];
    
        vec3 c0 = project - v[0];
        vec3 c1 = project - v[1];
        vec3 c2 = project - v[2];
        
        float d0 = dot(normal, cross(e0, c0));
        float d1 = dot(normal, cross(e1, c1));
        float d2 = dot(normal, cross(e2, c2));
    
        bool inside = d0 > 0 && d1 > 0 && d2 > 0;
        
        if (inside) {
            *pos_ptr = project_orig;
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
#ifdef DEBUG_TRACE_TRISEARCH
            draw_line(debug, center, center + normal * 5, vec4(0, 0, 1, 1));
            draw_point(debug, pos, vec4(0, 0, 1, 1));

            suspend_execution(debug);
#endif
            break;
        }
    }
       
    return 0;
}

#include "core/memory/linear_allocator.h"

extern float sliver_threshold;

void draw_mesh(CFDDebugRenderer& debug, SurfaceTriMesh& mesh) {
    LinearRegion region(get_temporary_allocator());
    bool* two_edges = TEMPORARY_ZEROED_ARRAY(bool, mesh.tri_count*3);
    
    for (tri_handle tri : mesh) {
        if (mesh.is_deallocated(tri)) continue;

        vec3 verts[3];
        mesh.triangle_verts(tri, verts);
        draw_triangle(debug, verts, vec4(1));
        vec3 p0 = (verts[0] + verts[1] + verts[2]) / 3.0f;

#if 0
        vec3 area = cross(verts[1] - verts[0], verts[2] - verts[0]);
        if (length(area) < sliver_threshold) {
            suspend_execution(debug);
            vec3 normal = triangle_normal(verts);
            for (uint i = 0; i < 3; i++) {
                draw_line(debug, verts[i] - normal, verts[i] + normal, RED_DEBUG_COLOR);
            }
            suspend_execution(debug);            
            printf("Triangle sliver!\n");
        }
#endif
        continue;

        for (uint i = 0; i < 3; i++) {
            vertex_handle v0, v1;
            mesh.edge_verts(tri + i, &v0, &v1);
            if (v0.id > v1.id) continue;

            EdgeSet edge_set = { v0,v1 };

            edge_handle neigh = mesh.edges[tri + i];
            mesh.edge_verts(neigh, &v0, &v1);
            if (neigh == 0) {
                draw_line(debug, p0, p0 + vec3(0, 1, 0), vec4(1, 0, 0, 1));
                continue;
            } 

            if (mesh.is_deallocated(TRI(neigh))) {
                draw_triangle(debug, verts, RED_DEBUG_COLOR);
                suspend_execution(debug);
                printf("Connected to dead triangle!\n");
            }

#if 1
            if (two_edges[neigh] || !(edge_set == EdgeSet{v0, v1})) {
                draw_edge(debug, mesh, tri + i, RED_DEBUG_COLOR);
                suspend_execution(debug);
                printf("Connectivity error!!!!!\n");
            }
#endif
            vec3 p1 = (mesh.positions[v0.id] + mesh.positions[v1.id]) / 2.0f;
            vec3 p2 = mesh.triangle_center(TRI(neigh));

            draw_line(debug, p0, p1, RED_DEBUG_COLOR);
            draw_line(debug, p1, p2, RED_DEBUG_COLOR);

            two_edges[neigh] = true;
        }
    }

}

//STABLE EDGES
edge_handle SurfaceTriMesh::get_edge(stable_edge_handle stable) {
    edge_handle result = stable_to_edge[stable.id];
    if (!result) {
        //draw_mesh(debug, *this);
        //suspend_execution(debug);
    }
    return result;
}

tri_handle SurfaceTriMesh::get_tri(stable_edge_handle stable) {
    return TRI(get_edge(stable));
}

stable_edge_handle SurfaceTriMesh::get_stable(edge_handle edge) {
    return edge_to_stable[edge];
}

edge_handle SurfaceTriMesh::get_opp_edge(stable_edge_handle edge) {
    return edges[stable_to_edge[edge.id]];
}

stable_edge_handle SurfaceTriMesh::get_opp(stable_edge_handle edge) {
    return edge_to_stable[edges[stable_to_edge[edge.id]]];
}


void SurfaceTriMesh::map_stable_edge(edge_handle edge, stable_edge_handle stable) {
    stable_to_edge[stable.id] = edge;
    edge_to_stable[edge] = stable;
}

void SurfaceTriMesh::mark_new_edge(edge_handle e0, bool both) {
    edge_handle e[2] = { e0, edges[e0] };

    for (uint i = 0; i < 1+both; i++) {
        edge_handle edge = e[i];
        stable_edge_handle handle;

        if (free_stable_edge.id == 0) {
            uint capacity = stable_to_edge.capacity;
            handle = { stable_to_edge.length };
            stable_to_edge.append({ edge });

            uint new_capacity = stable_to_edge.capacity;

            if (new_capacity > capacity) {
                edge_flags = realloc_t(edge_flags, new_capacity);
                memset(edge_flags + capacity, 0, sizeof(char) * (new_capacity - capacity));
            }
        }
        else {
            handle = free_stable_edge;
            free_stable_edge = { stable_to_edge[handle.id] };

            stable_to_edge[handle.id] = edge;
        }

        edge_to_stable[edge] = handle;
    };
}

void SurfaceTriMesh::remove_edge(edge_handle e0) {
    //todo recycle stable ids
    edge_handle e1 = edges[e0];

    stable_edge_handle stable0 = edge_to_stable[e0];
    stable_edge_handle stable1 = edge_to_stable[e1];

    stable_to_edge[stable0.id] = free_stable_edge.id;
    stable_to_edge[stable1.id] = stable0.id;

    free_stable_edge = stable1;
}

//DEBUGGING
void draw_triangle(CFDDebugRenderer& debug, SurfaceTriMesh& surface, tri_handle tri, vec4 color) {
    vec3 v[3];
    surface.triangle_verts(tri, v);
    draw_triangle(debug, v, color);
}

void draw_edge(CFDDebugRenderer& debug, SurfaceTriMesh& surface, edge_handle edge, vec4 color) {
    vec3 p0, p1;
    surface.edge_verts(edge, &p0, &p1);
    draw_line(debug, p0, p1, color);
}