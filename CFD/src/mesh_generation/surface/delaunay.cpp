#include "core/container/hash_map.h"
#include "mesh/surface_tet_mesh.h"
#include "mesh/feature_edges.h"
#include "visualization/debug_renderer.h"
#include "geo/predicates.h"

struct EdgeCavity {
    edge_handle neigh;
    stable_edge_handle edge;
    vertex_handle v0, v1;
    //char flags;
};

struct SurfaceDelaunay {
    CFDDebugRenderer& debug;
    SurfaceTriMesh& mesh;
    
    tri_handle last = 0;
    vector<tri_handle> stack;
    vector<EdgeCavity> cavities;
    hash_map<EdgeSet, edge_handle, 2000> shared_edges;
};

void destroy_surface_delaunay(SurfaceDelaunay* delaunay) {
    delete delaunay;
}

static bool debug_delaunay = false;

/*
tri_handle find_enclosing_tet(SurfaceDelaunay& d, tri_handle start, vec2 pos) {
    edge_handle last = 0;
    tri_handle current = start;
    uint count = 0;

loop: {
        uint offset = rand();
        for (uint it = 0; it < 3; it++) {
            uint i = (it + offset) % 3;

            edge_handle edge = current + i;
            if (edge == last) continue;

            edge_handle neigh = d.mesh.edges[edge];
            if (!neigh) continue;

            vec2 v0 = d.uvs[current];
            vec2 v1 = d.uvs[current + (i + 1) % 3];

            real det = orient2d(&v0.x, &v1.x, &pos.x);
            if (det > 0) {
                last = neigh;
                current = TRI(neigh);
                
                if (count++ > 1000) {
                    return 0;
                }

                goto loop;
            }
        }
    }
    
    return current;
}
*/

void draw_boundary(CFDDebugRenderer& debug, SurfaceTriMesh& mesh) {
    for (uint i = mesh.N; i <= mesh.tri_count * mesh.N; i++) {
        if (mesh.is_boundary(i)) {
            draw_edge(debug, mesh, i, RED_DEBUG_COLOR);
        }
    }
}

tri_handle insert_point(SurfaceDelaunay& d, tri_handle start, vec3 pos, const real min_dist) {
    float min_dist_sq = min_dist * min_dist;

    vertex_handle v = { d.mesh.positions.length }; //todo move into function
    d.mesh.positions.append(pos);

    d.mesh.dealloc_tri(start);
    d.stack.append(start);

    if (debug_delaunay) {
        clear_debug_stack(d.debug);
        draw_point(d.debug, pos, vec4(0, 1, 0, 1));
    }

    d.cavities.clear();
    d.shared_edges.clear();

    //debug_delaunay = true;

    while (d.stack.length > 0) {
        tri_handle current = d.stack.pop();

        vec3 v[3];
        d.mesh.triangle_verts(current, v);
        vec3 normal1 = triangle_normal(v);

        if (debug_delaunay) {
            draw_triangle(d.debug, v, RED_DEBUG_COLOR);
        }

        for (uint i = 0; i < 3; i++) {
            edge_handle edge = current + i;
            edge_handle neigh_edge = d.mesh.edges[edge];
            edge_handle neigh = d.mesh.TRI(neigh_edge);

            bool visited = d.mesh.is_deallocated(neigh);
            if (visited) continue;

            vertex_handle v0, v1;
            vec3 p0, p1;
            d.mesh.edge_verts(edge, &v0, &v1);
            d.mesh.edge_verts(edge, &p0, &p1);
            EdgeSet set(v0, v1);

            if (neigh) {
                vec3 v[4];
                for (uint i = 0; i < 3; i++) v[i] = d.mesh.position(neigh, i);

                vec3 normal = triangle_normal(v);
                vec3 tangent = normalize(v[1] - v[0]);
                vec3 bitangent = normalize(cross(normal, tangent));

                //float disp = dot(pos - v[0], normal);
                //vec3 proj = pos - normal*disp;

                v[3] = pos;

                vec2 uv[4];
                uv[0] = vec2();
                for (uint i = 1; i < 4; i++) {
                    vec3 dir = v[i] - v[0];
                    uv[i] = vec2(dot(dir, tangent), dot(dir, bitangent));
                }

                bool edge_vert = acosf(dot(normal, normal1)) > to_radians(20);
                //; d.vert_flags[v0.id] & EDGE_VERT&& d.vert_flags[v1.id] & EDGE_VERT;

                real det = incircle(&uv[0].x, &uv[1].x, &uv[2].x, &uv[3].x);
                bool inside = det > FLT_EPSILON && !d.mesh.is_boundary(edge); //&& fabs(disp) < 0.01;

                if (debug_delaunay) {
                    float offset = 10;
                    draw_triangle(d.debug,
                        vec3(uv[0].x, offset, uv[0].y),
                        vec3(uv[1].x, offset, uv[1].y),
                        vec3(uv[2].x, offset, uv[2].y),
                        RED_DEBUG_COLOR);
                    draw_triangle(d.debug, v, inside ? vec4(1, 0.5, 0.5, 1) : vec4(1, 1, 0, 1));
                    draw_point(d.debug, vec3(uv[3].x, offset, uv[3].y), inside ? vec4(0, 1, 0, 1) : vec4(0, 0, 1, 1));
                    //suspend_execution(d.debug);
                }

                if (inside) {
                    d.mesh.remove_edge(edge);
                    d.mesh.dealloc_tri(neigh);
                    d.stack.append(neigh);
                }
                else {
                    //for (uint i = 0; i < 5; i++) draw_line(d.debug, p0, p1, vec4(0, 0, 1, 1));
                    d.cavities.append({ neigh_edge, d.mesh.get_stable(edge), v0, v1 });
                }
            }
            else {
                d.cavities.append({ 0, d.mesh.get_stable(edge), v0, v1 });
            }
        }
    }

    //suspend_execution(d.debug);

    if (d.cavities.length < 3) {
        suspend_execution(d.debug);
        clear_debug_stack(d.debug);
        draw_mesh(d.debug, d.mesh);
        suspend_execution(d.debug);
    }


    for (EdgeCavity& cavity : d.cavities) {
        tri_handle tri = d.mesh.alloc_tri();
        //d.mesh.flags[tri / 3] = cavity.flags;
        d.mesh.indices[tri + 0] = cavity.v0;
        d.mesh.indices[tri + 1] = cavity.v1;
        d.mesh.indices[tri + 2] = v;

        d.mesh.edges[tri + 0] = cavity.neigh;
        d.mesh.edges[cavity.neigh] = tri + 0;

        d.mesh.map_stable_edge(tri + 0, cavity.edge);
        d.mesh.mark_new_edge(tri + 1, false);
        d.mesh.mark_new_edge(tri + 2, false);

        if (debug_delaunay) {
            vec3 v[3];
            d.mesh.triangle_verts(tri, v);
            if (d.mesh.is_boundary(d.mesh.get_edge(cavity.edge))) {
                draw_triangle(d.debug, v, vec4(0, 0, 1, 1));
                suspend_execution(d.debug);
                draw_boundary(d.debug, d.mesh);
                suspend_execution(d.debug);
            }
        }

        for (uint i = 1; i < 3; i++) {
            edge_handle edge = tri + i;

            vertex_handle v0, v1;
            d.mesh.edge_verts(tri + i, &v0, &v1);

            EdgeSet set(v0, v1);
            edge_handle& neigh = d.shared_edges[set];
            if (!neigh) {
                neigh = edge;
                d.mesh.edges[edge] = 0;
            }
            else {
                d.mesh.edges[neigh] = edge;
                d.mesh.edges[edge] = neigh;
            }
        }

        d.last = tri;
    }

    return start;
}

enum class MoveEdgeVert {
    None,
    Last,
    Next
};


vector<stable_edge_handle> insert_edges(SurfaceDelaunay& d, slice<FeatureCurve> curves, float mesh_size) {
    CFDDebugRenderer& debug = d.debug;
    SurfaceTriMesh& mesh = d.mesh;

    clear_debug_stack(debug);

    vector<stable_edge_handle> edges;

    for (FeatureCurve curve : curves) {
        Spline spline = curve.spline;

        float length = spline.total_length;
        uint n = floorf(length / mesh_size);
        float rem = length - n * mesh_size;
        rem /= 2.0f;

        float adapted = length / n;

        //printf("============== %i\n", spline.points());

        uint last_segment = UINT_MAX;
        MoveEdgeVert last_moved_vert = MoveEdgeVert::None; // MoveEdgeVert::Next;

        stable_edge_handle e_segment0;

        for (uint i = 0; i < n; i++) {
            bool is_last = i == n;

            //i == 0 ? 0 : rem + mesh_size * i
            float t = spline.const_speed_reparam(i, adapted);
            vec3 pos = spline.at_time(t);
            //if (is_last) pos = in.position(in.edges[curve.edges[curve.edges.length - 1]]);
            //vec3 tangent = normalize(spline.tangent(t));

           

            uint segment = (uint)t;

            if (segment - last_segment > 1) {
                for (uint i = last_segment + 1; i <= segment; i++) {
                    edge_handle edge = curve.edges[i];
                    draw_edge(debug, mesh, edge, RED_DEBUG_COLOR);
                }
            }            
            
            bool same_segment = last_segment == segment;

            if (!same_segment) e_segment0 = { curve.edges[segment] };
            stable_edge_handle e_segment1 = mesh.get_opp(e_segment0);

            edge_handle e0 = mesh.get_edge(e_segment0);
            edge_handle e1 = mesh.get_edge(e_segment1);

            vec3 v0 = mesh.position(e0);
            vec3 v1 = mesh.position(e1);

            float edge_length = ::length(v1 - v0);
            float dist1 = sq(v0 - pos);
            float dist2 = sq(v1 - pos);

            float threshold = fminf(edge_length, mesh_size) * 0.4;
            threshold = threshold*threshold; //distance is sq
            // && (dist1 < threshold || dist2 < threshold);

            bool closer_to_v0 = dist1 < dist2;

            //rethink this logic
            MoveEdgeVert move_edge_vert = {};
            if (!same_segment && last_moved_vert != MoveEdgeVert::Next) {
                move_edge_vert = MoveEdgeVert::Last; // closer_to_v0 ? MoveEdgeVert::Last : MoveEdgeVert::Next;
            }
            else if (dist2 < threshold) {
                move_edge_vert = MoveEdgeVert::Next;
            }

            vec4 color = move_edge_vert == MoveEdgeVert::None ? vec4(0, 0, 1, 1) : vec4(1, 0, 0, 1);
            //draw_line(debug, pos, pos + normal, color);


            if (move_edge_vert == MoveEdgeVert::Last) {
                mesh.positions[mesh.indices[e0].id] = pos;
            }
            else if (move_edge_vert == MoveEdgeVert::Next) {
                mesh.positions[mesh.indices[e1].id] = pos;
                e_segment0 = { curve.edges[segment + 1] };
            }
            else {
                e_segment1 = mesh.split_edge(e0, pos);
                mesh.mark_boundary(e_segment1);
                mesh.mark_boundary(mesh.get_opp(e_segment1));
                
                mesh.flip_bad_edges(mesh.get_tri(e_segment0), false);
                mesh.flip_bad_edges(mesh.get_tri(mesh.get_opp(e_segment0)), false);
                mesh.flip_bad_edges(mesh.get_tri(e_segment1), false);
                mesh.flip_bad_edges(mesh.get_tri(mesh.get_opp(e_segment1)), false);
                

                e_segment0 = e_segment1;
            }

            edges.append(e_segment0);

            last_segment = segment;
            last_moved_vert = move_edge_vert;

#if 0
            //clear_debug_stack(debug);
            draw_point(debug, pos, i == 0 ? RED_DEBUG_COLOR : GREEN_DEBUG_COLOR);
            //draw_mesh(debug, mesh);
            //draw_edge(debug, mesh, mesh.get_edge(e_segment1), RED_DEBUG_COLOR);
            suspend_execution(debug);
#endif
        }
    }

    return edges;
}

real tri_quality(vec3 v[3], real mesh_size) {
    float edge_length = 0;
    float longest_edge = 0;
    float shortest_edge = FLT_MAX;

    for (uint i = 0; i < 3; i++) {
        vec3 v0 = v[i];
        vec3 v1 = v[(i + 1) % 3];

        real edge = length(v1 - v0);

        longest_edge = fmaxf(longest_edge, edge);
        shortest_edge = fminf(shortest_edge, edge);
        edge_length += edge;
    }
    edge_length /= 3.0f;

    if (edge_length < mesh_size) return 1.0;
    if (edge_length > 4 / 3 * mesh_size) return 0;

    //float size_quality = 1.0 - (edge_length - mesh_size) / mesh_size;
    float angle_quality = shortest_edge / longest_edge;

    return angle_quality;
}

void flip_bad_edges(SurfaceTriMesh& mesh) {
    for (tri_handle tri : mesh) {
        mesh.flip_bad_edges(tri, false);
    }
}

void refine(SurfaceDelaunay& d, float mesh_size) {
    CFDDebugRenderer& debug = d.debug;
    SurfaceTriMesh& out = d.mesh;

    flip_bad_edges(out);

    clear_debug_stack(debug);
    draw_mesh(debug, out);
    suspend_execution(debug);

    uint N = d.mesh.N;
    uint n = 10;
    for (uint i = 0; i < n; i++) {
        uint n_tri = out.tri_count;
        for (uint tri = N; tri < n_tri * N; tri += N) {
            vec3 v[3];
            out.triangle_verts(tri, v);

            float quality = tri_quality(v, mesh_size);

            if (quality < 0.6) {
                vec3 ac = v[2] - v[0];
                vec3 ab = v[1] - v[0];
                vec3 abXac = cross(ab, ac);

                // this is the vector from a TO the circumsphere center
                vec3 to_circum = (cross(abXac, ab) * sq(ac) + cross(ac, abXac) * sq(ab)) / (2.f * sq(abXac));
                float circum_radius = length(to_circum);

                // The 3 space coords of the circumsphere center then:
                vec3 ccs = v[0] + to_circum; // now this is the actual 3space location

                tri_handle enclosed = out.project(debug, tri, &ccs, nullptr);
                if (!enclosed) continue;

                draw_point(debug, ccs, RED_DEBUG_COLOR);

                debug_delaunay = false;
                insert_point(d, enclosed, ccs, 0.001);

            }
        }
        
        
    }               
    
    out.smooth_mesh(2);
    flip_bad_edges(out); 
}

vector<stable_edge_handle> remesh_surface(SurfaceTriMesh& mesh, CFDDebugRenderer& debug, slice<FeatureCurve> curves, float uniform_mesh_size) {
    for (FeatureCurve curve : curves) {
        for (edge_handle edge : curve.edges) {
            stable_edge_handle stable = mesh.get_stable(edge);

            mesh.mark_boundary(stable);
            mesh.mark_boundary(mesh.get_opp(stable));
        }
    }

    SurfaceDelaunay d{ debug, mesh };
    mesh.debug = &debug;

    draw_mesh(debug, mesh);
    draw_boundary(debug, mesh);
    suspend_execution(debug);
    auto edges = insert_edges(d, curves, uniform_mesh_size);

    
    //suspend_execution(debug);

    refine(d, uniform_mesh_size * 1.5);

    suspend_execution(debug);
    clear_debug_stack(debug);
    draw_mesh(debug, mesh);    
    suspend_execution(debug);

    return edges;
}