#include "mesh/surface_tet_mesh.h"
#include "core/container/hash_map.h"

float sliver_threshold = 0.0001;

void SurfaceTriMesh::link_edge(edge_handle edge, edge_handle neigh) {
    //edge_handle prev_edge = out.edges[neigh]; //prev_edge -------> neigh
    //stable_edge_handle stable = get_stable(prev_edge);
    //map_stable_edge(edge, get_stable(prev_edge));
    assert(edge >= 3);
    assert(neigh >= 3);
    
    edges[edge] = neigh;
    edges[neigh] = edge;
    
    stable_edge_handle e_stable = get_stable(edge);
    stable_edge_handle n_stable = get_stable(neigh);
    
    //edge_flags[e_stable.id] |= edge_flags[n_stable.id];
    //edge_flags[n_stable.id] |= edge_flags[e_stable.id];

    vertex_handle v0, v1;
    edge_verts(neigh, &v0, &v1);

    indices[edge] = v1;
    indices[next_edge(edge)] = v0;
}    


//todo naming
edge_handle SurfaceTriMesh::flip_edge(edge_handle edge0, bool force) {
    edge_handle edge1 = edges[edge0];        
    
    /*if (front.contains(out.get_stable(edge0)) || front.contains(out.get_stable(edge1))) {
        clear_debug_stack(debug);
        draw_mesh(debug, out);
        draw_edge(debug, out, edge0, RED_DEBUG_COLOR);
        vec3 p0, p1;
        out.edge_verts(edge0, &p0, &p1);
        
        draw_line(debug, p0, p1 + 3*out.triangle_normal(TRI(edge0)), RED_DEBUG_COLOR);
        suspend_execution(debug);
    } */  
    
    vec3 p[4];
    diamond_verts(edge0, p);

    vec3 normal0 = cross(p[0] - p[1], p[0] - p[2]);
    vec3 normal1 = cross(p[3] - p[1], p[3] - p[2]);
    vec3 normal2 = cross(p[0] - p[1], p[0] - p[3]);

    const float sliver_threshold_sq = sliver_threshold * sliver_threshold;
    
    bool valid = dot(normal0, normal1) > 0
        && dot(normal0, normal2) > 0;
    if (!force) {
        valid = valid && dot(normal1, normal1) > sliver_threshold_sq
                      && dot(normal2, normal2) > sliver_threshold_sq;
    }
    if (!valid) return 0;

    vec3 normal = triangle_normal(TRI(edge0));

    edge_handle n0 = next_edge(edge0, 1);
    edge_handle n1 = next_edge(edge0, 2);
    edge_handle n2 = next_edge(edge1, 1);
    edge_handle n3 = next_edge(edge1, 2);

    edge_handle neigh0 = edges[n0];
    edge_handle neigh1 = edges[n1];
    edge_handle neigh2 = edges[n2];
    edge_handle neigh3 = edges[n3];

    remove_edge(edge0);

    bool flip = false;

    map_stable_edge(edge0, get_stable(flip ? n3 : n2));
    map_stable_edge(edge1, get_stable(flip ? n1 : n0));

    link_edge(edge0, flip ? neigh3 : neigh2);
    link_edge(edge1, flip ? neigh1 : neigh0);

    edge0 = next_edge(edge0, flip ? 2 : 1);
    edge1 = next_edge(edge1, flip ? 2 : 1);
    
    link_edge(edge0, edge1);
    mark_new_edge(edge0);

    return edge0;
}

stable_edge_handle SurfaceTriMesh::split_edge(edge_handle e0, vec3 pos) {
    //may be more correct to delete e0 since e0 is split in 2
    //remove_edge(e0);
       
    edge_handle e1 = edges[e0];
    
    //split triangle
    tri_handle t2 = alloc_tri();
    tri_handle t3 = alloc_tri();

    //draw_triangle(debug, result, t0, vec4(1));
    //draw_triangle(debug, result, t1, vec4(1));

    vertex_handle v = { (int)positions.length };
    positions.append(pos);

    edge_handle en0 = next_edge(e0);
    edge_handle en1 = next_edge(e1, 2);

    vec3 p4, p5;
    vec3 p6, p7;

    edge_verts(en0, &p4, &p5);
    edge_verts(en1, &p6, &p7);

    //draw_line(debug, p4, p5, RED_DEBUG_COLOR);
    //draw_line(debug, p6, p7, RED_DEBUG_COLOR);


    indices[t2] = v;
    indices[t3] = v;

    map_stable_edge(t2 + 1, get_stable(en0));
    map_stable_edge(t3 + 1, get_stable(en1));

    link_edge(t2 + 1, edges[en0]);
    link_edge(t3 + 1, edges[en1]);
    //3 verts are set

    link_edge(t2, t3 + 2);


    link_edge(en0, t2 + 2);
    link_edge(en1, t3);    
    
    //mark_new_edge(e0);
    mark_new_edge(t2);
    mark_new_edge(en0);
    mark_new_edge(t3);

    //draw_triangle(debug, result, t0, vec4(0,0,1,1));
    //draw_triangle(debug, result, t1, vec4(0,1,0,1));
    //draw_triangle(debug, result, t2, vec4(1,0,0,1));
    //draw_triangle(debug, result, t3, vec4(1,1,1,1));
    //suspend_execution(debug);

    //flip_bad_edge(TRI_NEXT_EDGE(e[0], 2), true);
    //flip_bad_edge(TRI_NEXT_EDGE(e[1], 1));

    return get_stable(t2);
}

stable_edge_handle SurfaceTriMesh::split_and_flip(edge_handle edge0, vec3 pos) {
    stable_edge_handle result = split_edge(edge0, pos);
    edge_handle edge2 = get_edge(result);

    flip_bad_edges(TRI(edge0));
    flip_bad_edges(TRI(edges[edge0]));
    flip_bad_edges(TRI(edge2));
    flip_bad_edges(TRI(edges[edge2]));

    return result;
}

bool SurfaceTriMesh::flip_bad_edge(edge_handle edge0) {
    edge_handle edge1 = edges[edge0];

    if (is_boundary(edge0) || is_boundary(edge1)) return false;

    vec3 v0, v1;
    edge_verts(edge0, &v0, &v1);
    //draw_line(debug, v0, v1, vec4(0, 0, 1, 1));
    //suspend_execution(debug);

    auto angle = [](SurfaceTriMesh& mesh, edge_handle edge, vec3 normal) {
        vec3 v0 = mesh.position(mesh.next_edge(edge, 2));
        vec3 v1 = mesh.position(edge);
        vec3 v2 = mesh.position(mesh.next_edge(edge, 1));
        //draw_line(debug, v0, v1, vec4(1, 0, 0, 1));
        //draw_line(debug, v0, v2, vec4(0, 1, 0, 1));
        //suspend_execution(debug);

        float angle = vec_angle(v1,v0,v2);
        return angle;
    };

    vec3 normal1 = triangle_normal(TRI(edge0));
    vec3 normal2 = triangle_normal(TRI(edge1));

    float dihedral = to_degrees(vec_angle(normal1, normal2));
    if (dihedral > 45) return false;

    float sum = angle(*this, edge0, normal1) + angle(*this, edge1, normal2);
    bool bad = sum >= PI;

    if (bad) bad = flip_edge(edge0) > 0;
    return bad;
}

#include <algorithm>

array<100, vertex_handle> verts_from_edge(SurfaceTriMesh& surface, slice<edge_handle> edges) {
    array<100, vertex_handle> result;
    result.resize(edges.length);
    
    for (uint i = 0; i < edges.length; i++) {
        result[i] = surface.indices[surface.edges[edges[i]]];
    }
    
    std::sort(result.begin(), result.end(), [](vertex_handle a, vertex_handle b) {
        return a.id < b.id;
    });
    
    return result;
}

bool SurfaceTriMesh::collapse_edge(edge_handle edge) {
    edge_handle neigh = edges[edge];
    
    auto ccw_edge = ccw(edge);
    auto ccw_neigh = ccw(neigh);
    
    array<100, vertex_handle> v_edge = verts_from_edge(*this, ccw_edge);
    array<100, vertex_handle> v_neigh = verts_from_edge(*this, ccw_neigh);
    
    bool e_boundary = false;
    bool n_boundary = false;
    
    for (edge_handle edge : ccw_edge){
        if (is_boundary(edge)) {
            e_boundary = true;
            break;
        }
    }
    for (edge_handle neigh : ccw_neigh) {
        if (is_boundary(neigh)) {
            n_boundary = true;
            break;
        }
    }
    
    uint shared = 0;
    
    uint i = 0;
    uint j = 0;
    while (i < v_edge.length && j < v_neigh.length) {
        vertex_handle e0 = v_edge[i];
        vertex_handle e1 = v_neigh[j];
        
        if (e1.id > e0.id) i++;
        else if (e1.id < e0.id) j++;
        else {
            shared++;
            i++;
            j++;
        }
    }
    
    if (shared != 2 || (e_boundary && n_boundary)) return false;
    
    edge_handle e0 = edges[next_edge(edge,1)];
    edge_handle e1 = edges[next_edge(edge,2)];
    edge_handle e2 = edges[next_edge(neigh,1)];
    edge_handle e3 = edges[next_edge(neigh,2)];
    
    
    bool debug_collapse = false;
     
    if (debug_collapse) {
        draw_triangle(*debug, *this, TRI(neigh), RED_DEBUG_COLOR);
        draw_triangle(*debug, *this, TRI(edge), RED_DEBUG_COLOR);
        draw_triangle(*debug, *this, TRI(e0), GREEN_DEBUG_COLOR);
        draw_triangle(*debug, *this, TRI(e1), GREEN_DEBUG_COLOR);
        draw_triangle(*debug, *this, TRI(e2), GREEN_DEBUG_COLOR);
        draw_triangle(*debug, *this, TRI(e3), GREEN_DEBUG_COLOR);
        suspend_execution(*debug);
    }
    
    vertex_handle v = indices[edge];
    vec3 p0 = position(edge);
    vec3 p1 = position(neigh);
    vec3 p2 = (e_boundary == n_boundary) ? (p0 + p1) / 2 : (e_boundary ? p0 : p1);
    
    if (!move_vert(edge, p2, 0.4)) return false;
    
    for (edge_handle edge : ccw_neigh) {
        indices[edge] = v;
        if (debug_collapse) draw_edge(*debug, *this, edge, BLUE_DEBUG_COLOR);
    }
    
    
    if (debug_collapse) {
        draw_point(*debug, p2, GREEN_DEBUG_COLOR);
        suspend_execution(*debug);
    }
    
    stable_edge_handle s0 = get_stable(e0);
    stable_edge_handle s1 = get_stable(e1);
    stable_edge_handle s2 = get_stable(e2);
    stable_edge_handle s3 = get_stable(e3);
    
    edge_flags[s0.id] |= edge_flags[s1.id];
    edge_flags[s1.id] = edge_flags[s0.id];
    edge_flags[s1.id] |= edge_flags[s2.id];
    edge_flags[s2.id] = edge_flags[s1.id];
    
    link_edge(e0, e1);
    link_edge(e2, e3);
    
    dealloc_tri(edge);
    dealloc_tri(neigh);
    
    flip_bad_edges(TRI(e0));
    flip_bad_edge(TRI(e1));
    flip_bad_edges(TRI(e2));
    flip_bad_edge(TRI(e3));
    
    return true;
}

#include "visualization/debug_renderer.h"
#include "core/memory/linear_allocator.h"
#include "visualization/color_map.h"

void SurfaceTriMesh::smooth_mesh(uint n) {
    LinearRegion region(get_temporary_allocator());
    uint* vert_smoothed = TEMPORARY_ARRAY(uint, tri_count / 32);
    bool mixed_mesh = N != 3;

    for (uint it = 0; it < n; it++) {
        memset(vert_smoothed, 0, sizeof(uint) * tri_count / 32);
        
        for (uint tri = N; tri < tri_count * N; tri += N) {
            if (is_deallocated(tri)) continue;              
            bool quad = is_quad(tri);    

            vec3 verts[4];

            uint edges = quad ? 4 : 3;

            for (uint j = 0; j < edges; j++) {
                uint i = tri + j;

                uint mask = 1 << (i % 32);
                
                vertex_handle v = indices[i];

                vec3 new_pos;
                float total_weight = 0.0f;

                vec3 orig_pos = positions[v.id]; 
                verts[j] = orig_pos;
                
                if (vert_smoothed[i / 32] & mask) continue;

                bool boundary = false;
                for (edge_handle edge : ccw(i)) {
                    if (is_boundary(edge)) {
                        boundary = true;
                        break;
                    }

                    if (quad) draw_edge(*debug, *this, edge, RED_DEBUG_COLOR);

                    vec3 pos = position(next_edge(edge));

                    float weight = 1.0; // length(pos - orig_pos);
                    new_pos += weight * pos;
                    total_weight += weight;
                }

                if (!boundary) {
                    new_pos /= total_weight;

                    verts[j] = lerp(orig_pos, new_pos, 0.1);
                }

                vert_smoothed[i / 32] |= mask;
            }

            for (uint j = 0; j < edges; j++) {
                positions[indices[tri + j].id] = verts[j];
            }
        }
    }
}


bool SurfaceTriMesh::move_vert(edge_handle dir, vec3 new_pos, real min) {
    vertex_handle v = indices[dir];
    vec3 orig_pos = positions[v.id];

    auto ccw_edges = ccw(dir);
    vec3 normals[100];

    bool debug_move = false; // dir == 3528;

    if (debug_move) {
        clear_debug_stack(*debug);
    }

    for (uint i = 0; i < ccw_edges.length; i++) {
        edge_handle edge = ccw_edges[i];
        vec3 v[3];
        triangle_verts(TRI(edge), v);

        normals[i] = cross(v[1] - v[0], v[2] - v[0]);

        if (debug_move) {
            draw_triangle(*debug, v, RED_DEBUG_COLOR);
        }
    }

    if (debug_move) {    
        draw_point(*debug, orig_pos, RED_DEBUG_COLOR);
        draw_point(*debug, new_pos, BLUE_DEBUG_COLOR);
        suspend_execution(*debug);
    }

    float factor = 1.0f;
    while (factor > min) {
        bool flipped_triangle = false;

        positions[v.id] = factor * new_pos + (1.0 - factor) * orig_pos;

        for (uint i = 0; i < ccw_edges.length; i++) {
            edge_handle edge = ccw_edges[i];
            vec3 v[3];
            triangle_verts(TRI(edge), v);

            vec3 new_normal = cross(v[1] - v[0], v[2] - v[0]);
            if (dot(new_normal, normals[i]) < 0) {
                flipped_triangle = true;
                break;
            }
        }

        if (debug_move) {
            clear_debug_stack(*debug);
            draw_mesh(*debug, *this);
            draw_point(*debug, orig_pos, RED_DEBUG_COLOR);
            draw_point(*debug, new_pos, BLUE_DEBUG_COLOR);
            suspend_execution(*debug);
        }

        if (!flipped_triangle) return true;

        //debug_move = true;

        factor -= 0.2;
    }

    positions[v.id] = orig_pos;
    
    return false;
}

void SurfaceTriMesh::flip_bad_edges(tri_handle start, bool smooth) {
    hash_set<tri_handle, 100> visited;
    hash_set<vertex_handle, 100> smoothed;
    array<100, tri_handle> stack;
    uint count = 0;

    start = TRI(start);
    stack.append(start);
    while (stack.length > 0) {
        tri_handle current = stack.pop();
        if (debug) draw_triangle(*debug, *this, current, RED_DEBUG_COLOR);

        for (uint i = 0; i < 3; i++) {
            vertex_handle v = indices[current + i];

            if (smooth && smoothed.index(v) == -1) {
                vec3 new_pos;
                float total_weight = 0.0f;

                vec3 orig_pos = positions[v.id];

                bool boundary = false;
                for (edge_handle edge : ccw(current + i)) {
                    if (is_boundary(edge)) {
                        boundary = true;
                        break;
                    }

                    vec3 pos = position(next_edge(edge));

                    float weight = 1.0; // length(pos - orig_pos);
                    new_pos += weight * pos;
                    total_weight += weight;
                }

                if (!boundary) {               
                    new_pos /= total_weight;

                    move_vert(current + i, new_pos);
                    smoothed.add(v);
                }
            }
            
            tri_handle neigh = neighbor(current, i);
            if (visited.index(neigh) != -1) continue;


            bool flip = flip_bad_edge(current + i);
            if (flip) {
                stack.append(neigh);
                visited.add(neigh);
                if (count++ > 80) return;
            }
        }
    }
}

