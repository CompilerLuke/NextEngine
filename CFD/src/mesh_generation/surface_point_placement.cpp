#include "mesh/surface_tet_mesh.h"
#include "mesh_generation/surface_point_placement.h"
#include "mesh_generation/cross_field.h"
#include "mesh_generation/point_octotree.h"
#include "mesh/input_mesh_bvh.h"
#include "numerical/spline.h"
#include "mesh/feature_edges.h"

#include "visualization/debug_renderer.h"

#include "geo/predicates.h"

#include <glm/gtx/rotate_vector.hpp>
#include "visualization/color_map.h"
#include "core/profiler.h"

#include <deque>

SurfacePointPlacement::SurfacePointPlacement(SurfaceTriMesh& tri_mesh, SurfaceCrossField& cross, PointOctotree& octotree, tvector<vec3>& shadow_points, slice<float> curvatures)
: mesh(tri_mesh), cross_field(cross), octo(octotree), curvatures(curvatures), shadow_points(shadow_points) {

}

vertex_handle add_vertex(PointOctotree& octo, CFDDebugRenderer& debug, tvector<vec3>& shadow, vec3 normal, vec3 pos) {
    vertex_handle v = {(int)octo.positions.length};
    octo.positions.append(pos);
    octo.add_vert(v);

    float threshold = 0.001;

    vec3 shadow_point = pos - normalize(normal) * threshold;
    shadow.append(shadow_point);

    draw_line(debug, pos, shadow_point);

    return v;
}

float distance_to_triangle(vec3 v[3], vec3 pos) {
    vec3 center = (v[0] + v[1] + v[2]) / 3.0f;
    vec3 normal = triangle_normal(v);
    
    vec3 project = pos - dot(pos - center, normal)*normal;
    
    vec3 c0 = (v[0]+v[1])/2;
    vec3 c1 = (v[1]+v[2])/2;
    vec3 c2 = (v[2]+v[0])/2;
    
    vec3 e0 = v[1]-v[0];
    vec3 e1 = v[2]-v[1];
    vec3 e2 = v[0]-v[2];
    
    vec3 b0 = -normalize(cross(normal, e0));
    vec3 b1 = -normalize(cross(normal, e1));
    vec3 b2 = -normalize(cross(normal, e2));
    
    float disp_e0 = dot(project - c0, b0);
    float disp_e1 = dot(project - c1, b1);
    float disp_e2 = dot(project - c2, b2);
    
    float distance = FLT_MAX;
    if (disp_e0 > 0 && disp_e0 < distance) distance = disp_e0;
    else if (disp_e1 > 0 && disp_e1 < distance) distance = disp_e1;
    else if (disp_e2 > 0 && disp_e2 < distance) distance = disp_e2;
    else distance = 0.0f; //inside
    
    return distance;
}

void link_edge(SurfaceTriMesh& mesh, edge_handle edge, edge_handle neigh) {
    mesh.edges[edge] = neigh;
    mesh.edges[neigh] = edge;

    vertex_handle v0, v1;
    mesh.edge_verts(neigh, &v0, &v1);

    mesh.indices[edge] = v1;
    mesh.indices[TRI_NEXT_EDGE(edge)] = v0;
}

void draw_triangle(CFDDebugRenderer& debug, SurfaceTriMesh& mesh, tri_handle tri, vec4 color) {
    vec3 points[3];
    mesh.triangle_verts(tri, points);
    vec3 normal = triangle_normal(points);
    
    //draw_line(debug, points[0], points[1], color);
    //draw_line(debug, points[1], points[2], color);
    //draw_line(debug, points[2], points[0], color);

    for (uint i = 0; i < 3; i++) points[i] += normal * 0.0001;
    draw_triangle(debug, points, color);
}

enum class MoveEdgeVert {
    None,
    Last,
    Next
};

struct QMorphFront {
    struct Edge {
        tri_handle tm0, tm1; //mesh triangle index
        edge_handle edge;
    };

    std::deque<Edge> edges;
};

void draw_mesh(CFDDebugRenderer& debug, SurfaceTriMesh& mesh) {
    for (tri_handle tri : mesh) {
        if (mesh.is_deallocated(tri)) continue;

        vec3 verts[3];
        mesh.triangle_verts(tri, verts);
        draw_triangle(debug, verts, vec4(1));

        vec3 p0 = (verts[0] + verts[1] + verts[2]) / 3.0f;

        for (uint i = 0; i < 3; i++) {
            vertex_handle v0, v1;
            mesh.edge_verts(tri + i, &v0, &v1);
            if (v0.id > v1.id) continue;

            tri_handle neigh = mesh.neighbor(tri, i);
            if (neigh == 0) {
                draw_line(debug, p0, p0 + vec3(0, 1, 0), vec4(1, 0, 0, 1));
                continue;
            }

            vec3 p1 = (mesh.positions[v0.id] + mesh.positions[v1.id]) / 2.0f;
            vec3 p2 = mesh.triangle_center(neigh);

            draw_line(debug, p0, p1, RED_DEBUG_COLOR);
            draw_line(debug, p1, p2, RED_DEBUG_COLOR);
        }
    }

}

void flip_edge(SurfaceTriMesh& mesh, edge_handle edge0, bool flip) {
    edge_handle edge1 = mesh.edges[edge0];

    edge_handle neigh0 = mesh.edges[TRI_NEXT_EDGE(edge0, 1)];
    edge_handle neigh1 = mesh.edges[TRI_NEXT_EDGE(edge0, 2)];

    edge_handle neigh2 = mesh.edges[TRI_NEXT_EDGE(edge1, 1)];
    edge_handle neigh3 = mesh.edges[TRI_NEXT_EDGE(edge1, 2)];

    link_edge(mesh, edge0, flip ? neigh3 : neigh2);
    link_edge(mesh, edge1, flip ? neigh1 : neigh0);
    link_edge(mesh, TRI_NEXT_EDGE(edge0, flip ? 2 : 1), TRI_NEXT_EDGE(edge1, flip ? 2 : 1));
}

void flip_bad_edge(SurfaceTriMesh& mesh, CFDDebugRenderer& debug, edge_handle edge0, bool flip = false) {
    edge_handle edge1 = mesh.edges[edge0];

    vec3 v0, v1;
    mesh.edge_verts(edge0, &v0, &v1);
    draw_line(debug, v0, v1, vec4(0,0,1,1));
    suspend_execution(debug);

    auto angle = [&](SurfaceTriMesh& mesh, edge_handle edge) {
        vec3 v0 = mesh.position(TRI_NEXT_EDGE(edge, 2)); 
        vec3 v1 = mesh.position(edge);
        vec3 v2 = mesh.position(TRI_NEXT_EDGE(edge, 1));

        vec3 v01 = v1 - v0;
        vec3 v02 = v2 - v0;

        //draw_line(debug, v0, v1, vec4(1, 0, 0, 1));
        //draw_line(debug, v0, v2, vec4(0, 1, 0, 1));
        //suspend_execution(debug);

        float angle = acosf(dot(v01, v02) / (length(v01) * length(v02)));
        return angle;
    };

    float sum = angle(mesh, edge0) + angle(mesh, edge1);
    bool bad = sum >= PI;

    if (bad) {
        flip_edge(mesh, edge0, flip);
    }
}

/*
void insert_edges(SurfaceTriMesh& mesh, CFDDebugRenderer& debug, SurfaceCrossField& cross_field, slice<FeatureCurve> curves) {
    float mesh_size = 0.2;
    SurfaceTriMesh result = mesh;
    QMorphFront front;
    
    for (FeatureCurve curve : curves) {
        Spline spline = curve.spline;

        float length = spline.total_length;
        uint n = length / mesh_size;

        //printf("============== %i\n", spline.points());

        uint last_segment = UINT_MAX;
        MoveEdgeVert last_moved_vert = MoveEdgeVert::None;

        edge_handle e[2];

        for (uint i = 0; i < n; i++) {
            float t = spline.const_speed_reparam(i, mesh_size);
            vec3 pos = spline.at_time(t);
            vec3 tangent = spline.tangent(t);

            uint segment = (uint)t;

            edge_handle e_segment[2];
            e_segment[0] = curve.edges[segment];
            e_segment[1] = mesh.edges[e_segment[0]];

            vec3 normal = cross_field.at_edge(e_segment[0]).normal;
            vec3 bitangent0 = cross_field.at_edge(e_segment[0]).bitangent;
            vec3 bitangent1 = cross_field.at_edge(e_segment[1]).bitangent;

            bool same_segment = last_segment == segment;
            if (!same_segment) {
                e[0] = e_segment[0];
                e[1] = e_segment[1];
            }

            vec3 v0 = result.position(e[0]);
            vec3 v1 = result.position(e[1]);

            float edge_length = ::length(v0 - v1);

            tri_handle t0 = TRI(e[0]);
            tri_handle t1 = TRI(e[1]);

            float dist1 = sq(v0 - pos);
            float dist2 = sq(v1 - pos);

            //float threshold = fminf(edge_length, mesh_size) * 0.4;
            //threshold = threshold*threshold; //distance is sq
            // && (dist1 < threshold || dist2 < threshold);

            bool closer_to_v0 = dist1 < dist2;

            MoveEdgeVert move_edge_vert = {};
            if (!same_segment && last_moved_vert != MoveEdgeVert::Next) {
                move_edge_vert = closer_to_v0 ? MoveEdgeVert::Last : MoveEdgeVert::Next;
            }

            vec4 color = move_edge_vert == MoveEdgeVert::None ? vec4(0, 0, 1, 1) : vec4(1, 0, 0, 1);
            //draw_line(debug, pos, pos + normal, color);

            if (move_edge_vert == MoveEdgeVert::Last) result.positions[result.indices[e[0]].id] = pos;
            else if (move_edge_vert == MoveEdgeVert::Next) result.positions[result.indices[e[1]].id] = pos;
            else {
                //split triangle
                tri_handle t2 = result.alloc_tri();
                tri_handle t3 = result.alloc_tri();

                //draw_triangle(debug, result, t0, vec4(1));
                //draw_triangle(debug, result, t1, vec4(1));

                vertex_handle v = { (int)result.positions.length };
                result.positions.append(pos);

                edge_handle en0 = TRI_NEXT_EDGE(e[0]);
                edge_handle en1 = TRI_NEXT_EDGE(e[1], 2);

                vec3 p4, p5;
                vec3 p6, p7;

                result.edge_verts(en0, &p4, &p5);
                result.edge_verts(en1, &p6, &p7);

                draw_line(debug, p4, p5, RED_DEBUG_COLOR);
                draw_line(debug, p6, p7, RED_DEBUG_COLOR);


                result.indices[t2] = v;
                result.indices[t3] = v;

                link_edge(result, t2 + 1, result.edges[en0]);
                link_edge(result, t3 + 1, result.edges[en1]);
                //3 verts are set

                link_edge(result, t2, t3 + 2);

                link_edge(result, en0, t2 + 2);
                link_edge(result, en1, t3);

                //draw_triangle(debug, result, t0, vec4(0,0,1,1));
                //draw_triangle(debug, result, t1, vec4(0,1,0,1));
                //draw_triangle(debug, result, t2, vec4(1,0,0,1));
                //draw_triangle(debug, result, t3, vec4(1,1,1,1));
                //suspend_execution(debug);

                flip_bad_edge(result, debug, TRI_NEXT_EDGE(e[0], 2), true);
                flip_bad_edge(result, debug, TRI_NEXT_EDGE(e[1], 1));

                e[0] = t2;
                e[1] = result.edges[t2];

                //clear_debug_stack(debug);
                //draw_mesh(debug, result);


                flip_bad_edge(result, debug, t2 + 1);
                flip_bad_edge(result, debug, t3 + 1, true);

                clear_debug_stack(debug);
                draw_mesh(debug, result);
                suspend_execution(debug);
            }

            front.edges.push_back({ TRI(e_segment[0]), TRI(e_segment[0]), e[0] });
            front.edges.push_back({ TRI(e_segment[1]), TRI(e_segment[1]), e[1] });

            last_segment = segment;
            last_moved_vert = move_edge_vert;
        }
    }
}
*/

#include "core/container/hash_map.h"
#include "geo/predicates.h"

struct EdgeCavity {
    edge_handle neigh;
    vertex_handle v0, v1;
    //char flags;
};

struct SurfaceDelaunay {
    SurfaceTriMesh& mesh;
    CFDDebugRenderer& debug;
    tri_handle last;
    vector<char> vert_flags;
    vector<tri_handle> stack;
    vector<EdgeCavity> cavities;
    hash_map<EdgeSet, edge_handle, 100> shared_edges;
    uint input_watermark;
};

#include <unordered_map>

static bool debug_delaunay = false;

const char INPUT_VERT = 1 << 0;
const char EDGE_VERT = 1 << 1;

tri_handle insert_point(SurfaceDelaunay& d, vec3 pos, const real min_dist, bool stop_at_edge = true) {

    
    tri_handle start = d.mesh.project(d.debug, d.last, &pos, nullptr); //todo create find function
    if (!start) return 0;
    assert(start != 0);

    float min_dist_sq = min_dist*min_dist;

    for (uint i = 0; i < 3; i++) {
        vec3 vertex_pos = d.mesh.position(start, i);
        real dist = sq(vertex_pos - pos);
        if (dist < min_dist_sq) {
            vertex_handle v = d.mesh.indices[start + i];
            char& flag = d.vert_flags[v.id];
            bool input_vert = flag & INPUT_VERT;
            if (input_vert) {
                //d.mesh.positions[d.mesh.indices[start + i].id] = pos;
                flag ^= INPUT_VERT;
            }
            return input_vert ? start : 0;
        }
    }

    vertex_handle v = { d.mesh.positions.length }; //todo move into function
    d.mesh.positions.append(pos);
    d.vert_flags.append(stop_at_edge ? 0 : EDGE_VERT);

    d.mesh.dealloc_tri(start);
    d.stack.append(start);

    if (debug_delaunay) {
        clear_debug_stack(d.debug);
        draw_point(d.debug, pos, vec4(0, 1, 0, 1));
    }

    d.cavities.clear();
    d.shared_edges.clear();

    while (d.stack.length > 0) {
        tri_handle current = d.stack.pop();

        vec3 v[3];
        d.mesh.triangle_verts(current, v);
        vec3 normal1 = triangle_normal(v);

        if (debug_delaunay) {
            draw_triangle(d.debug, v, RED_DEBUG_COLOR);
        }


        char flag = d.mesh.flags[current / 3];
        for (uint i = 0; i < 3; i++) {
            tri_handle edge = current + i;
            tri_handle neigh_edge = d.mesh.edges[edge];
            tri_handle neigh = TRI(neigh_edge);

            bool visited = d.mesh.is_deallocated(neigh);
            if (visited) continue;

            vertex_handle v0, v1;
            vec3 p0, p1;
            d.mesh.edge_verts(edge, &v0, &v1);
            d.mesh.edge_verts(edge, &p0, &p1);
            EdgeSet set(v0,v1);

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
                bool inside = det > FLT_EPSILON && !(edge_vert && stop_at_edge); //&& fabs(disp) < 0.01;

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
                    d.mesh.dealloc_tri(neigh);
                    d.stack.append(neigh);
                }
                else {
                    for (uint i = 0; i < 5; i++) draw_line(d.debug, p0, p1, vec4(0, 0, 1, 1));
                    d.cavities.append({neigh_edge, v0, v1 });
                }
            }
            else {
                d.cavities.append({ 0, v0, v1 });
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

    std::unordered_map<u64, edge_handle> shared_edges;

    for (EdgeCavity& cavity : d.cavities) {
        tri_handle tri = d.mesh.alloc_tri();
        //d.mesh.flags[tri / 3] = cavity.flags;
        d.mesh.indices[tri + 0] = cavity.v0;        
        d.mesh.indices[tri + 1] = cavity.v1;
        d.mesh.indices[tri + 2] = v;

        d.mesh.edges[tri + 0] = cavity.neigh;
        d.mesh.edges[cavity.neigh] = tri + 0;

        if (debug_delaunay) {
            vec3 v[3];
            d.mesh.triangle_verts(tri, v);
            draw_triangle(d.debug, v, vec4(0, 0, 1, 1));
            suspend_execution(d.debug);
        }

        for (uint i = 1; i < 3; i++) {
            edge_handle edge = tri + i;

            vertex_handle v0, v1;
            d.mesh.edge_verts(tri + i, &v0, &v1);

            EdgeSet set(v0, v1);
            edge_handle& neigh = shared_edges[hash_func(set)]; 
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

void qmorph(SurfaceTriMesh& mesh, CFDDebugRenderer& debug, SurfaceCrossField& cross_field, slice<FeatureCurve> curves) {
    QMorphFront front;
    SurfaceTriMesh result = mesh;
    SurfaceDelaunay d = { result, debug };
    
    float mesh_size = 0.15;
    
    for (FeatureCurve curve : curves) {
        Spline& spline = curve.spline;
        float length = spline.total_length;
        uint n = length / mesh_size;

        tri_handle start = TRI(curve.edges[0]);
        if (!result.is_deallocated(start)) d.last = start;

        for (uint i = 0; i < n; i++) {
            float t = spline.const_speed_reparam(i, mesh_size);
            vec3 pos = spline.at_time(t);

            uint segment = min(t, curve.edges.length-1);
            
            insert_point(d, pos, 0.7*mesh_size);
            
            clear_debug_stack(debug);
            draw_mesh(debug, result);
            suspend_execution(debug);
        }

        
    }

    clear_debug_stack(debug);
    draw_mesh(debug, result);
    suspend_execution(debug);

    while (!front.edges.empty()) {
        auto edge = front.edges.front();
        front.edges.pop_front();

        vec3 p0 = result.position(edge.edge);
        vec3 p1 = result.position(result.edges[edge.edge]);

        Cross cross0 = cross_field.at_tri(edge.tm0, p0);
        Cross cross1 = cross_field.at_tri(edge.tm1, p1);

        vec3 p01 = p0 - p1;

        auto compute_bitangent = [](Cross cross, vec3 edge) {
            float tangent_dot = dot(edge, cross.tangent);
            float bitangent_dot = dot(edge, cross.bitangent);
        
            return fabs(tangent_dot) > fabs(bitangent_dot) ? cross.bitangent * signnum(tangent_dot) : cross.tangent * signnum(bitangent_dot);
        };

        vec3 bitangent0 = compute_bitangent(cross0, p01);
        vec3 bitangent1 = compute_bitangent(cross1, p01);

        float factor = 1.0f;

        tri_handle tr2, tr3;
        vec3 p2, p3;

        while (factor > 0.0f) {
            p2 = p0 + bitangent0 * factor * mesh_size;
            p3 = p1 + bitangent1 * factor * mesh_size;

            draw_line(debug, p0, p2, RED_DEBUG_COLOR);
            draw_line(debug, p1, p3, RED_DEBUG_COLOR);

            suspend_execution(debug);

            tr2 = result.project(debug, TRI(edge.edge), &p2, nullptr);
            tr3 = result.project(debug, TRI(edge.edge), &p3, nullptr);

            factor -= 0.2f;
        }

        if (factor < 0.0f) continue;

        tri_handle tm2 = mesh.project(debug, edge.tm0, &p2, nullptr);
        tri_handle tm3 = mesh.project(debug, edge.tm1, &p3, nullptr);

        auto insert_point = [&](tri_handle tri, vec3 point) {
            vertex_handle verts[3];
            tri_handle neighs[3];
            vec3 positions[3];

            result.triangle_verts(tri, verts);
            result.triangle_verts(tri, positions);
            for (uint i = 0; i < 3; i++) neighs[i] = result.neighbor_edge(tri, i);
            
            float threshold = 0.1;
            for (uint i = 0; i < 3; i) {
                float dist = sq(positions[i] - point);
                if (dist < threshold * threshold) {
                    result.positions[verts[i].id] = point;
                    return;
                }
            }
            
            vertex_handle v = { result.positions.length };
            result.positions.append(point);

            tri_handle t0 = tri;            
            tri_handle t1 = result.alloc_tri();            
            tri_handle t2 = result.alloc_tri();

            //T0
            result.indices[t0 + 2] = v;

            //T1
            link_edge(mesh, t1 + 0, neighs[1]);
            link_edge(mesh, t1 + 2, t0 + 1);

            //T2
            link_edge(mesh, t2 + 0, neighs[2]);
            link_edge(mesh, t2 + 1, t0 + 2);
            link_edge(mesh, t2 + 2, t1 + 1);
        };

        insert_point(tr2, p2);
        insert_point(tr2, p3);

        draw_line(debug, p0, p2, RED_DEBUG_COLOR);
        draw_line(debug, p1, p3, RED_DEBUG_COLOR);
    }

    for (tri_handle tri : result) {
        vec3 verts[3];
        result.triangle_verts(tri, verts);
        draw_triangle(debug, verts, vec4(1));
    }

    suspend_execution(debug);
}

void propagate_points(PointOctotree& octo, SurfaceTriMesh& mesh, SurfaceCrossField& cross_field, CFDDebugRenderer& debug, tvector<vec3>& shadow_points, slice<float> curvatures, slice<FeatureCurve> curves) {
    SurfaceTriMesh result = mesh;
    SurfaceDelaunay d = { result, debug };
    d.vert_flags.resize(result.positions.length);
    d.input_watermark = d.vert_flags.length;

    for (char& flag : d.vert_flags) {
        flag = INPUT_VERT;
    }

    for (FeatureCurve curve : curves) {
        for (edge_handle edge0 : curve.edges) {
            edge_handle edge1 = mesh.edges[edge0];

            vertex_handle v0 = mesh.indices[edge0];
            vertex_handle v1 = mesh.indices[edge1];

            d.vert_flags[v0.id] |= EDGE_VERT;
            d.vert_flags[v1.id] |= EDGE_VERT;
        }
    }
    
    struct Point {
        tri_handle tri_in;
        tri_handle tri_out;
        vec3 position;
        float dist;
        float last_dt;
    };

    uint watermark = octo.positions.length;

    std::deque<Point> point_queue;

    //suspend_execution(debug);
    //clear_debug_stack(debug);

    float mesh_size = 0.15;
    float dist_mult = 0;  0.5;
    float curv_mult = 0.02;

    
    for (uint i = 1; i < mesh.positions.length; i++) {
        vec3 pos = mesh.positions[i];
        float curv = curvatures[i];

        draw_point(debug, pos, color_map(fabs(curv)/1.0));
    }

    suspend_execution(debug);

    Profile discretize("Discretize curves");

    for (FeatureCurve curve : curves) {
        Spline spline = curve.spline;

        float length = spline.total_length;
        float x = 0.0f;
        //uint n = length / mesh_size;

        //printf("============== %i\n", spline.points());

        tri_handle start = TRI(curve.edges[0]);
        if (!result.is_deallocated(start)) d.last = TRI(curve.edges[0]);

        while (x <= length) {
            float t = spline.const_speed_reparam(x, 1.0);
            vec3 pos = spline.at_time(t);
            
            vec3 tangent = normalize(spline.tangent(t));

            edge_handle e0 = curve.edges[min(t, curve.edges.length-1)];
            edge_handle e1 = mesh.edges[e0];

            float curvature0 = curvatures[mesh.indices[e0].id];
            float curvature1 = curvatures[mesh.indices[e1].id];
            float curvature = (curvature0 + curvature1) / 2.0f;
            float dt = mesh_size / (1 + curv_mult * fabs(curvature));
            
            bool end = x == length;
            x += dt;
            if (!end && x > length) x = length;

            Cross cross1 = cross_field.at_edge(e0);
            Cross cross2 = cross_field.at_edge(e1);
            
            //if (octo.find_closest(pos, cross1, 0.7*dt).id != -1) continue;
            //if (octo.find_closest(pos, cross2, 0.5*mesh_size).id != -1) continue;

            vec3 normal0 = cross1.normal;
            vec3 normal1 = cross2.normal;

            vec3 bitangent0 = normalize(cross(normal0, tangent));
            vec3 bitangent1 = normalize(cross(normal1, tangent));
            
            tri_handle tri_out = insert_point(d, pos, 0.7 * mesh_size, false);
            if (!tri_out) continue;

            

            //add_vertex(octo, debug, shadow_points, normal0 + normal1, pos);
            //draw_point(debug, pos, vec4(1,0,0,1));
            //draw_line(debug, pos, pos + tangent * mesh_size, vec4(1,0,0,1));
            //draw_line(debug, pos, pos - tangent * mesh_size, vec4(1,0,0,1));

            //draw_line(debug, pos, pos - bitangent0 * mesh_size, vec4(0,0,1,1));
            //draw_line(debug, pos, pos + bitangent1 * mesh_size, vec4(0,0,1,1));

            draw_point(debug, pos, vec4(0, 1, 0, 1));

            Point point0{ TRI(e0), tri_out, pos + bitangent0 * mesh_size, mesh_size, mesh_size };
            Point point1{ TRI(e1), tri_out, pos - bitangent1 * mesh_size, mesh_size, mesh_size };

            draw_point(debug, point0.position, vec4(1, 0, 0, 1));
            draw_point(debug, point1.position, vec4(1, 0, 0, 1));

            point_queue.push_front(point0);
            point_queue.push_front(point1);
        }
    }

    suspend_execution(debug);
    clear_debug_stack(debug);
    draw_mesh(debug, result);
    suspend_execution(debug);

    discretize.log();

    debug_delaunay = false;

    //suspend_execution(debug);

    Profile place_points("Place points");

    while (!point_queue.empty()) {
        Point point = point_queue.front();
        point_queue.pop_front();

        tri_handle tri_in = mesh.project(debug, point.tri_in, &point.position, nullptr);
        if (!tri_in) continue;

        clear_debug_stack(debug);

        d.last = point.tri_out;
        tri_handle tri_out = insert_point(d, point.position, 0.7*mesh_size);
        if (!tri_out) continue;
        tri_out = d.last;

        Cross cross = cross_field.at_tri(tri_in, point.position);
        float dt = mesh_size;

        /*
        tri_handle tri = mesh.project(debug, point.tri, &point.position, nullptr);
        if (!tri) continue;

        float curvature = curvatures[mesh.indices[tri].id];
        float last_dt = point.last_dt;
        float dt = mesh_size * (1.0 + dist_mult * point.dist) / (1 + curv_mult *fabs(curvature));

        Cross cross = cross_field.at_tri(tri, point.position);

        vertex_handle found = octo.find_closest(point.position, cross, 0.7 * dt); // / 2.0f);
        if (found.id != -1) {
            //draw_point(debug, point.position, vec4(0,0,1,1));
            continue;
        }

        //suspend_execution(debug);
        add_vertex(octo, debug, shadow_points, cross.normal, point.position);
        */

        
        clear_debug_stack(debug);
        draw_point(debug, point.position, vec4(1, 0, 0, 1)); // color_map(dt));
        draw_mesh(debug, result);
        suspend_execution(debug);

        //cross.bitangent = normalize(::cross(cross.normal, cross.tangent));

        vec3 normal = cross.normal;

        Point points[4] = {};
        points[0].position = point.position + cross.tangent * dt;
        points[1].position = point.position - cross.tangent * dt;
        points[2].position = point.position + cross.bitangent * dt;
        points[3].position = point.position - cross.bitangent * dt;

        for (uint i = 0; i < 4; i++) {
            points[i].tri_in = tri_in;
            points[i].tri_out = tri_out;
            points[i].dist = point.dist + dt;
            points[i].last_dt = dt;
            draw_point(debug, points[i].position, vec4(0,0,1,1));
            point_queue.push_back(points[i]);
        }

        suspend_execution(debug);

        //point_queue += {points,4};
    }

    place_points.log();

    draw_mesh(debug, result);

    //suspend_execution(debug);

    //for (uint i = watermark; i < octo.positions.length; i++) {
    //    draw_point(debug, octo.positions[i], vec4(1, 0, 0, 0));
    //}
    
    //suspend_execution(debug);
}

void SurfacePointPlacement::propagate(slice<FeatureCurve> curves, CFDDebugRenderer& debug) {
    //vector<FeatureEdge> features = cross_field.get_feature_edges();



    //suspend_execution(debug);
    
    //qmorph(mesh, debug, cross_field, curves);
    propagate_points(octo, mesh, cross_field, debug, shadow_points, curvatures, curves);
}