#include "mesh/surface_tet_mesh.h"
#include "mesh/feature_edges.h"
#include "mesh_generation/cross_field.h"
#include "visualization/debug_renderer.h"
#include "core/container/hash_map.h"
#include "core/math/intersection.h"
#include <glm/gtx/rotate_vector.hpp>
#include <deque>

constexpr uint RIGHT_EDGE = 1 << 0;
constexpr uint LEFT_EDGE = 1 << 1;
constexpr uint SEAM = 1 << 2;
constexpr uint GEN_2 = 1 << 3;
constexpr uint INSERTED_IN_FRONT = 1 << 8;

using front_edge_handle = stable_edge_handle;

#include <unordered_map>

struct QMorphFront {
    CFDDebugRenderer* debug;
    SurfaceTriMesh* out;

    struct Edge {
        tri_handle tm0 = 0, tm1 = 0;
        front_edge_handle left;
        front_edge_handle right;
        front_edge_handle prev;
        front_edge_handle next;
        uint state = 0;
    };

    bool current_gen = 0;

    //array has faster access but because only a small percentage of edges are part of the front, it will waste memory
    //hash_map_base<stable_edge_handle, Edge> edges;
    std::unordered_map<stable_edge_handle, Edge> edges;
    
    static constexpr uint N_PRIORITY = 4;

    front_edge_handle priority[2][N_PRIORITY] = {};
    front_edge_handle free = {};

    uint gen(uint state, bool next);
    void dealloc(front_edge_handle);
    void remove(front_edge_handle);
    void push(front_edge_handle);
    front_edge_handle pop();
    
    bool contains(front_edge_handle);
};

//QMorphFront::QMorphFront(uint n) {
    //todo: create RAII hash_map

    //edges.capacity = n;
    //edges.meta = new hash_meta[n];
    //edges.keys = new front_edge_handle[n];
    //edges.values = new QMorphFront::Edge[n];
//}

bool QMorphFront::contains(front_edge_handle edge) {
    return edges.find(edge) != edges.end();
}

uint QMorphFront::gen(uint state, bool next) {
    state &= ~GEN_2;
    if (next) state |= current_gen ? 0 : GEN_2;
    else state |= current_gen ? GEN_2 : 0;
    return state;
}

constexpr uint state_to_priority(uint state) {
    if (state & SEAM) return 2;
    if ((state & LEFT_EDGE) && (state & RIGHT_EDGE)) return 3;
    if ((state & LEFT_EDGE) || (state & RIGHT_EDGE)) return 1;
    return 0;
}

void QMorphFront::push(front_edge_handle handle) {
    Edge& edge = edges[handle];
    if (edge.state & INSERTED_IN_FRONT) return;

    if (edge.tm0 == 0 || edge.tm1 == 0) {
        printf("Did not find triangle!!\n");
    }

    bool gen = edge.state & GEN_2;
    uint state = state_to_priority(edge.state);
    front_edge_handle& head = priority[gen][state];

    edges[head].prev = handle;
    edge.next = head;
    edge.prev = {};
    edge.state |= INSERTED_IN_FRONT;

    out->mark_boundary(handle);
    out->mark_boundary(out->get_opp(handle));

    //printf("Pushing %i ---> %i\n", handle.id, head.id);

    head = handle;
}

front_edge_handle QMorphFront::pop() {        
    for (int i = N_PRIORITY - 1; i >= 0; i--) {
        for (int j = 0; j < 2; j++) {
            uint gen = current_gen ^ j;

            front_edge_handle& head = priority[gen][i];
            if (!head.id) continue;

            current_gen = gen;

            front_edge_handle result = head;
            head = edges[head].next;
            edges[head].prev = {};

            edges[result].state &= ~INSERTED_IN_FRONT;
            edges[result].next = {};
            edges[result].prev = {};

            return result;
        }
            
        //current_gen = !current_gen;
        //printf("Starting new generation!\n");
    }

    return {};
}



//todo create function in dictionary
void QMorphFront::dealloc(front_edge_handle handle) {
    remove(handle);

    edges.erase(handle);
    //printf("Erased %i\n", handle.id);

    //uint index = edges.index(handle);
    //edges.meta[index] = {};
    //edges.keys[index] = {};
    //edges.values[index] = {};
}

void QMorphFront::remove(front_edge_handle handle) {
    Edge& edge = edges[handle];
    if (!(edge.state & INSERTED_IN_FRONT)) return; //deleted

    if (edge.prev.id == 0) { //head of list
        uint index = state_to_priority(edge.state);
        
        front_edge_handle* head = &priority[current_gen][index];
        if (*head != handle) head = &priority[!current_gen][index];
        if (*head != handle) {
            draw_mesh(*debug, *out);
            suspend_execution(*debug);
        }

        *head = edges[*head].next;
        edges[*head].prev = {};
    }
    else {
        Edge& prev = edges[edge.prev];
        prev.next = edge.next;
        edges[edge.next].prev = edge.prev;


    }        
    //printf("Removing %i ---> %i --> %i\n", edge.prev, handle, edge.next);

    edge.state &= ~INSERTED_IN_FRONT;
    edge.next = {};
    edge.prev = {};
}

struct QMorph {
    SurfaceTriMesh& in;
    SurfaceCrossField& cross_field;
    CFDDebugRenderer& debug;

    QMorphFront front;

    SurfaceTriMesh out;

    float mesh_size = 0.2;

    vector<tri_handle> stack;
    
    vector<stable_edge_handle> intersections;

    //edge_handle find_edge(tri_handle start, vec3 pos, vec3 dir);
    //void find_intersecting_edges(edge_handle start, edge_handle end);

    stable_edge_handle recover_edge(edge_handle start, edge_handle end);

    edge_handle insert_into_triangle(tri_handle tri, vec3 point, bool flip = false);
    tri_handle find_tri(tri_handle start, vec3 pos);

    vector<QMorphFront> identify_edge_loops(slice<stable_edge_handle>);
       
    void seam(front_edge_handle);
    
    void propagate(QMorphFront&);

    void update_side_edge(QMorphFront& front, front_edge_handle, front_edge_handle, vec3 normal, uint side, bool push, bool delay = true);
};


void draw_front(CFDDebugRenderer& debug, SurfaceTriMesh& mesh, QMorph& qmorph, QMorphFront& front) {
    //clear_debug_stack(debug);
    printf("========== FRONT ===========\n");
    for (uint gen = 0; gen < 2; gen++) {
        uint current_gen = front.current_gen ^ gen;

        vec4 colors[4] = {
            vec4(0,0,0,1), //no side edges
            vec4(0,1,0,1), //right
            vec4(1,0,0,1), //left
            vec4(1,1,1,1), //both
        };

        for (uint p = 0; p < 4; p++) {
            stable_edge_handle edge = front.priority[current_gen][p];
            printf("====== PRIORITY %i\n", p);
            while (edge.id) {
                printf("---> %i\n", edge.id);
                bool next_gen = gen == 1;

                vec4 color = colors[front.edges[edge].state & 3];

                vec3 p0, p1;
                mesh.edge_verts(mesh.get_edge(edge), &p0, &p1);
                vec3 mid = (p0 + p1) / 2.0f;

                vec3 normal = mesh.triangle_normal(mesh.get_tri(edge));
                vec3 tangent = normalize(p1 - p0);
                vec3 bitangent = normalize(cross(normal, tangent));

                draw_line(debug, p0, p1, color); // next_gen ? RED_DEBUG_COLOR : color);
                
                float arrow_y = 0.02;
                float arrow_x = 0.02;

                draw_line(debug, mid, mid + bitangent * arrow_y - tangent * arrow_x, RED_DEBUG_COLOR);
                draw_line(debug, mid, mid - bitangent * arrow_y - tangent * arrow_x, RED_DEBUG_COLOR);
                
                edge = front.edges[edge].next;
            }
        }
    }
}

#include "geo/predicates.h"

tri_handle QMorph::find_tri(tri_handle start, vec3 pos) {
    start = out.TRI(start);

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
        out.triangle_verts(current, v);
        vec3 normal = triangle_normal(v);
        vec3 center = (v[0] + v[1] + v[2]) / 3.0f;

        float disp = dot(pos - center, normal);
        vec3 project_orig = pos - disp * normal;

        const float threshold = 0.01;

        vec3 pc = normalize(center - project_orig);
        vec3 project = project_orig; // +pc * threshold;

        vec3 e0 = v[1] - v[0];
        vec3 e1 = v[2] - v[1];
        vec3 e2 = v[0] - v[2];

        vec3 c0 = project - v[0];
        vec3 c1 = project - v[1];
        vec3 c2 = project - v[2];

        float d0 = dot(normal, cross(e0, c0));
        float d1 = dot(normal, cross(e1, c1));
        float d2 = dot(normal, cross(e2, c2));

        bool inside = d0 > 0 && d1 > 0 && d2 > 0;

        if (inside) {
            return current;
        }

        float min_length = FLT_MAX; // sq(project - center);
        tri_handle closest_neighbor = 0;

        for (uint j = 0; j < 3; j++) {
            edge_handle edge0 = current + j;
            edge_handle edge1 = out.edges[edge0];

            //todo: should only need to check one
            if (front.contains(out.get_stable(edge0)) || front.contains(out.get_stable(edge1))) continue;

            tri_handle tri = out.TRI(edge1);

            if (tri == last) continue;

            vec3 v0 = v[j];
            vec3 v1 = v[(j + 1) % 3];
            vec3 e = v0 - v1;
            vec3 c = (v0 + v1) / 2.0f;

            //Plane test with edge
            vec3 v2 = v0 + normal;

            if (orient3d(&v0.x, &v1.x, &v2.x, &pos.x) < 0) {
                closest_neighbor = tri;
                break;
            }

            //vec3 b = normalize(cross(normal, e));
            //float distance_to_edge = dot(project - c, b);

            /*if (distance_to_edge > 0 && distance_to_edge < min_length) {
                closest_neighbor = tri;
                min_length = distance_to_edge;
            }*/
        }

        if (closest_neighbor) {
            last = current;
            current = closest_neighbor;
        }
        else {
            break;
        }
    }

    return 0;
}

//#define VALIDATE_EACH_OP

edge_handle QMorph::insert_into_triangle(tri_handle tri, vec3 point, bool flip) {
    vec3 positions[3];
    vertex_handle verts[3];
    tri_handle neighs[3];
    out.triangle_verts(tri, positions);
    out.triangle_verts(tri, verts);

    for (uint i = 0; i < 3; i++) neighs[i] = out.edges[tri + i];

    vec3 normal = out.triangle_normal(tri);

    const float threshold = 0.01;
    const float threshold_sq = threshold * threshold;

    
    for (uint i = 0; i < 3; i++) {
        float dist_to_vert = sq(positions[i] - point);
        if (dist_to_vert < threshold_sq) {
            out.positions[verts[i].id] = point;
#ifdef VALIDATE_EACH_OP
            draw_mesh(debug, out);
#endif
            return tri + i;
        }
    }
    

    uint closest = UINT_MAX;
    float lowest_dist = 0.03;

    for (uint i = 0; i < 3; i++) {
        vec3 p0 = positions[i];
        vec3 p1 = positions[(i + 1) % 3];

        vec3 bitangent = normalize(cross(normal, p1 - p0));
        float dist_to_edge = fabs(dot(point - p0, bitangent));

        if (dist_to_edge < lowest_dist) {
            closest = i;
            lowest_dist = dist_to_edge;
        }
    }

    if (closest != UINT_MAX) {
        uint i = closest;
        edge_handle result;
        if (flip) result = out.get_edge(out.split_and_flip(tri + i, point));
        else result = out.get_edge(out.split_edge(tri + i, point));


        #ifdef VALIDATE_EACH_OP
        draw_mesh(debug, out);
        draw_point(debug, point, BLUE_DEBUG_COLOR);
        draw_edge(debug, out, tri + i, GREEN_DEBUG_COLOR);
        suspend_execution(debug);
        #endif
        return result;
    }

    vertex_handle v = { out.positions.length };
    out.positions.append(point);

    tri_handle t0 = tri;
    tri_handle t1 = out.alloc_tri();
    tri_handle t2 = out.alloc_tri();

    out.map_stable_edge(t1, out.get_stable(t0+1));
    out.map_stable_edge(t2, out.get_stable(t0+2));

    //T0
    out.indices[t0 + 2] = v;

    //T1
    out.link_edge(t1 + 0, neighs[1]);
    out.link_edge(t1 + 2, t0 + 1);

    //T2
    out.link_edge(t2 + 0, neighs[2]);
    out.link_edge(t2 + 1, t0 + 2);
    out.link_edge(t2 + 2, t1 + 1);

    out.mark_new_edge(t0+1);
    out.mark_new_edge(t0+2);
    out.mark_new_edge(t1+1);

    if (flip) {
        out.flip_bad_edges(t0);
        out.flip_bad_edges(t1);
        out.flip_bad_edges(t2);
    }

    //draw_triangle(debug, out, t0, RED_DEBUG_COLOR);
    //draw_triangle(debug, out, t1, RED_DEBUG_COLOR);
    //draw_triangle(debug, out, t2, RED_DEBUG_COLOR);
    //suspend_execution(debug);
#ifdef VALIDATE_EACH_OP
    draw_mesh(debug, out);
#endif
    return t0 + 2;
}

//todo naming
stable_edge_handle QMorph::recover_edge(edge_handle start, edge_handle end) {
    stable_edge_handle result = {};
    
    //Identify intersecting edges
    vertex_handle nvc = out.indices[start];
    vertex_handle nvd = out.indices[end];

    vec3 nc = out.position(start);
    vec3 nd = out.position(end);
    vec3 cd = nd - nc;

    /*if (fabs(1-nc.x) > 0.05) {
        suspend_execution(debug);
        draw_mesh(debug, out);
        suspend_execution(debug);
        draw_front(debug, out, *this, front);
        suspend_execution(debug);
    }*/

    //draw_mesh(debug, out);
    //draw_line(debug, nc, nd, BLUE_DEBUG_COLOR);
    //suspend_execution(debug);

    vec3 p0;

    edge_handle first_flip = 0;

    intersections.clear();

    edge_handle e0 = 0;
    edge_handle e1 = start;

    bool debug_vectors = false;

    vec3 start_normal;

    uint count = 0;
    while (true) {
        count++;
        e1 = out.next_edge(e1);
        assert(e0 != e1);

        //todo should be tangent plane to nc, not e1
        vec3 normal0 = out.triangle_normal(out.TRI(e1));
        //vec3 normal1 = out.triangle_normal(TRI(out.edges[e1]));
        vec3 normal = normal0;

        vertex_handle v0, v1;
        out.edge_verts(e1, &v0, &v1);

        vec3 p1;
        if (v0 == nvc) {
            p1 = out.positions[v1.id];
            if (v1 == nvd) return out.get_stable(e1);
        }
        else if (v1 == nvc) {
            p1 = out.positions[v0.id];
            if (v0 == nvd) return out.get_stable(out.edges[e1]);
        }
        else continue;

        if (!e0) start_normal = normal;
        
        vec3 e0_vec = normalize(cross(normal, p0 - nc));
        vec3 e1_vec = normalize(cross(normal, p1 - nc));

        vec3 cdt = cd - dot(cd, normal) * normal;
        
        if (debug_vectors) {
            clear_debug_stack(debug);
            draw_front(debug, out, *this, front);
            suspend_execution(debug);
            draw_edge(debug, out, e1, vec4(0,0,0,1));
            if (e0) {
                draw_edge(debug, out, e0, vec4(0,0,0,1));
            }
            draw_line(debug, nc, nd, RED_DEBUG_COLOR);
            suspend_execution(debug);
            draw_line(debug, nc, nc + 2 * cdt, RED_DEBUG_COLOR);
            draw_line(debug, nc, nc + e0_vec, GREEN_DEBUG_COLOR);
            draw_line(debug, nc, nc + e1_vec, BLUE_DEBUG_COLOR);
            draw_line(debug, nc, nc + normal, RED_DEBUG_COLOR);
            suspend_execution(debug);
        }

        if (e0 && dot(start_normal, normal) > 0.5 && dot(e0_vec, cdt) > 0 && dot(e1_vec, cdt) < 0) {
            first_flip = out.TRI(e0) + (3 - out.TRI_EDGE(e1) - out.TRI_EDGE(e0)); //diagonal
            
            if (debug_vectors) {
                draw_edge(debug, out, first_flip, RED_DEBUG_COLOR);
                suspend_execution(debug);
                
            }
            
            intersections.append(out.get_stable(first_flip));

            break;
        }

        e0 = out.edges[e1];
        e1 = e0;
        p0 = p1;

        if (e0 == out.next_edge(start) && e0) {
            debug_vectors = true;
            //printf("Looped back, found no flips!");
        }


        if (count > 20) {            
            debug_vectors = true;
            suspend_execution(debug);
            draw_mesh(debug, out);
            suspend_execution(debug);

        }
    }

    if (debug_vectors) {
        clear_debug_stack(debug);
        draw_mesh(debug, out);
    }

    while (true) {
        count++;
        tri_handle t0 = out.TRI(first_flip);
        first_flip = out.edges[first_flip];
        tri_handle t1 = out.TRI(first_flip);

        bool found = false;
        for (uint i = 0; i < 3; i++) {
            vertex_handle v = out.indices[t1 + i];
            if (v == nvd) {
                found = true;
                break;
            }
        }

        if (found) break;

        //vec3 normal0 = out.triangle_normal(t0);
        vec3 normal1 = out.triangle_normal(t1);
        vec3 normal = normal1; //normalize(normal0 + normal1);

        t0 = t1;

        vec3 n0 = out.position(t0, (out.TRI_EDGE(first_flip) + 2) % 3);
        vec3 v = cross(normal, n0 - nc);

        vec3 cdt = cd - dot(cd, normal) * normal;

        edge_handle f1 = out.next_edge(first_flip);
        edge_handle f2 = out.next_edge(first_flip, -1); //TRI_NEXT_EDGE(f1);

        if (debug_vectors) {
            suspend_execution(debug);
            draw_triangle(debug, out, t1, vec4(1, 1, 0.5, 1));
            draw_edge(debug, out, first_flip, RED_DEBUG_COLOR);
            draw_line(debug, nc, nd, RED_DEBUG_COLOR);
            draw_line(debug, n0, n0 + v, GREEN_DEBUG_COLOR);
            draw_line(debug, n0, n0 + cdt, BLUE_DEBUG_COLOR);        
            suspend_execution(debug);
        }

        if (dot(cdt, v) < 0) {
            first_flip = f1;
        }
        else {
            first_flip = f2;
        }
   
        //draw_edge(debug, out, first_flip, RED_DEBUG_COLOR);
        //suspend_execution(debug);
        intersections.append(out.get_stable(first_flip));

        if (count > 20) debug_vectors = true;
    }
    

    //clear_debug_stack(debug);
    //draw_mesh(debug, out);    
    //suspend_execution(debug);

    //flip diagonals
    clear_debug_stack(debug);
    /*if (start == 12465) {
        suspend_execution(debug);
        clear_debug_stack(debug);
        draw_mesh(debug, out);
        for (uint i = 0; i < intersections.length; i++) {
            draw_edge(debug, out, out.get_edge(intersections[i]), RED_DEBUG_COLOR);
            suspend_execution(debug);
        }
        suspend_execution(debug);
    }*/

    for (uint i = 0; i < intersections.length; i++) {
        stable_edge_handle handle = intersections[i];
        edge_handle flip_edge = out.get_edge(handle);
        if (flip_edge == 0) {
            printf("Missing Edge!\n");
        }
        draw_edge(debug, out, flip_edge, RED_DEBUG_COLOR);
        if (start == 3814) {
            suspend_execution(debug);
            clear_debug_stack(debug);
            draw_mesh(debug, out);
        }
        bool last = i == intersections.length - 1;
        edge_handle edge = out.flip_edge(flip_edge, last);
        if (start == 3814) {
            suspend_execution(debug);
            clear_debug_stack(debug);
            draw_mesh(debug, out);
            suspend_execution(debug);
        }
        if (!edge) { //flip failed
            if (i > 100 || i == intersections.length - 1) {
                printf("FAILED!!!!!\n");
                //clear_debug_stack(debug);
                suspend_execution(debug);
                draw_mesh(debug, out);
                draw_line(debug, nc, nd, RED_DEBUG_COLOR);
                draw_edge(debug, out, flip_edge, BLUE_DEBUG_COLOR);
                suspend_execution(debug);
                clear_debug_stack(debug);
                draw_front(debug, out, *this, front);
                draw_line(debug, nc, nd, RED_DEBUG_COLOR);
                draw_edge(debug, out, flip_edge, BLUE_DEBUG_COLOR);
                suspend_execution(debug);
            }
            intersections.append(handle);
        }
        else {
            result = out.get_stable(edge);
            
            //Normal
            vec3 normal0 = out.triangle_normal(out.TRI(edge));
            vec3 normal1 = out.triangle_normal(out.TRI(out.edges[edge]));
            vec3 normal = normalize(normal0 + normal1);

            //Intersection
            vec3 v0, v1;
            out.edge_verts(edge, &v0, &v1);

            if ((v0 == nc && v1 == nd) || (v1 == nc && v0 == nd)) continue;

            bool intersects = edge_edge_intersect(normal, v0, v1, nc, nd);

            //suspend_execution(debug);
            //draw_edge(debug, out, edge, outside ? BLUE_DEBUG_COLOR : GREEN_DEBUG_COLOR);
            //draw_line(debug, nc, nd, RED_DEBUG_COLOR);
            //suspend_execution(debug);

            if (intersects) intersections.append(result);
        }
    }


    vertex_handle v0 = out.indices[out.get_edge(result)];
    if (v0 != nvc) {
        result = out.get_opp(result);
    }

#ifdef VALIDATE_EACH_OP
    draw_mesh(debug, out);
#endif
    //clear_debug_stack(debug);
    //draw_mesh(debug, out);
    //suspend_execution(debug);
    //draw_edge(debug, out, get_edge(result), RED_DEBUG_COLOR);
    //suspend_execution(debug);

    return result;
}

const real MAX_CORNER = 0.65 * PI;

vector<QMorphFront> QMorph::identify_edge_loops(slice<stable_edge_handle> edges) {
    //BUILD EDGE GRAPH
    uint n = edges.length * 2;

    hash_map_base<vertex_handle, slice<stable_edge_handle>> feature_edge_graph;
    feature_edge_graph.capacity = n;
    feature_edge_graph.meta = new hash_meta[n]();
    feature_edge_graph.keys = new vertex_handle[n]();
    feature_edge_graph.values = new slice<stable_edge_handle>[n]();

    uint offset = 0;

    //suspend_execution(debug);    

    for (uint i = 0; i < edges.length; i++) {
        stable_edge_handle handle = edges[i];
        edge_handle edge = out.get_edge(handle);
    
        vertex_handle v0, v1;
        out.edge_verts(edge, &v0, &v1);

        draw_edge(debug, out, edge, RED_DEBUG_COLOR);
        
        feature_edge_graph[v0].length++;
        feature_edge_graph[v1].length++;
        offset += 2;
    }
    
    //suspend_execution(debug);
    draw_mesh(debug, out);
    suspend_execution(debug);

    stable_edge_handle* edges_graph = new stable_edge_handle[offset];

    offset = 0;
    for (uint i = 0; i < n; i++) {
        if (!feature_edge_graph.is_full(i)) continue;
        auto& slice = feature_edge_graph.values[i];

        slice.data = edges_graph + offset;
        offset += slice.length;
        
        slice.length = 0;
    }

    for (uint i = 0; i < edges.length; i++) {
        stable_edge_handle handle = edges[i];
        edge_handle edge = out.get_edge(handle);

        vertex_handle v0, v1;
        out.edge_verts(edge, &v0, &v1);
        draw_edge(debug, out, edge, vec4(RED_DEBUG_COLOR));

        auto& v0_edges = feature_edge_graph[v0];
        auto& v1_edges = feature_edge_graph[v1];

        stable_edge_handle& e0 = v0_edges.data[v0_edges.length++];
        stable_edge_handle& e1 = v1_edges.data[v1_edges.length++];

        e0 = handle;
        e1 = out.get_opp(handle); //v1 --> v0
    }

    vector<QMorphFront> result;
    

    //FIND EDGELOOPS
    for (uint i = 0; i < n; i++) {
        if (!feature_edge_graph.is_full(i)) continue;
        
        auto& edges = feature_edge_graph.values[i];
        if (edges.length == 0) continue;

        stable_edge_handle current = edges.data[--edges.length];
        edge_handle start = out.get_edge(current);

        //todo: fix magic number and make it resizeable
        //QMorphFront front(9999);

        front_edge_handle prev = {};
        front_edge_handle first = {};
        
        QMorphFront front{ &debug, &out };

        //clear_debug_stack(debug);

        while (current.id) {
            edge_handle edge = out.get_edge(current);
            front_edge_handle handle = current;

            draw_edge(debug, out, edge, RED_DEBUG_COLOR);

            vec3 p0, p1;
            out.edge_verts(edge, &p0, &p1);

            vertex_handle v = out.indices[out.next_edge(edge)];
            auto& neighbors = feature_edge_graph[v];
            vec3 normal = out.triangle_normal(out.TRI(edge));

            vec3 pos = out.positions[v.id];
            draw_line(debug, pos, pos + normal, GREEN_DEBUG_COLOR);            
            //suspend_execution(debug);
            
            edge_handle opp = out.edges[edge];
            int index = -1;

            float highest_dot = 0; // -FLT_MAX;

            uint count = 0;
            vec3 normal1;
            for (uint i = 0; i < neighbors.length; i++) {
                edge_handle edge = out.get_edge(neighbors[i]);
                if (edge == opp) {
                    continue;
                }
                normal1 = out.triangle_normal(out.TRI(edge));
                vec3 pos = out.position(out.next_edge(edge));

                float angle = dot(normal, normal1);

                if (angle > highest_dot) {
                    index = i;
                    highest_dot = angle;
                }
            }

            vec3 p2;

            if (index != -1) {
                current = neighbors[index];
                neighbors.data[index] = neighbors.data[--neighbors.length]; //avoid bounds checking as current may be the last element
                p2 = out.position(out.get_edge(current, 1));
            }
            else {
                current.id = 0;
                p2 = out.position(out.next_edge(start));
            }

            //clear_debug_stack(debug);
            //draw_line(debug, p0, p1, BLUE_DEBUG_COLOR);
            //draw_line(debug, p1, p2, RED_DEBUG_COLOR);

            float angle = vec_angle(p0, p1, p2); //vec_dir_angle(p0-p1, p2-p1, normal);
            bool right_state = angle < 0.65 * PI;

            /*if (right_state) {
                draw_line(debug, p0, p1, GREEN_DEBUG_COLOR);
                draw_line(debug, p1, p2, GREEN_DEBUG_COLOR);
                suspend_execution(debug);
            }*/
            //suspend_execution(debug);
                
            //todo more efficient to use pointer
            front.edges[handle].state = right_state;
            front.edges[handle].tm0 = in.get_tri(handle);
            front.edges[handle].tm1 = in.get_tri(handle);

            auto connect_last = [](QMorphFront& front, front_edge_handle handle, front_edge_handle prev) {
                bool left_state = front.edges[prev].state & RIGHT_EDGE;

                front.edges[handle].left = prev;
                front.edges[handle].state |= left_state << 1;

                front.edges[prev].right = handle;

                front.push(handle);
            };

            if (prev.id != 0) {
                connect_last(front, handle, prev);
                
                bool last = !current.id;
                if (last) {
                    front.edges[first].state |= right_state ? LEFT_EDGE : 0;
                    connect_last(front, first, handle);
                }
            }
            else {
                first = handle;
            }

            prev = handle;
        }

        

        //draw corners
#if 0
        suspend_execution(debug);
        front_edge_handle corner = front.priority[0][1];
        while (corner.id) {
            draw_edge(debug, out, get_edge(corner), RED_DEBUG_COLOR);
            corner = front.edges[corner].next;
        }
        suspend_execution(debug);
#endif

        result.append(std::move(front));
    }

    suspend_execution(debug);

    //todo create RAII hash_map wrapper
    delete feature_edge_graph.meta;
    delete feature_edge_graph.keys;
    delete feature_edge_graph.values;
    delete edges_graph;

    return result; // { std::move(front) };
}


#include "visualization/color_map.h"


void get_verts(SurfaceTriMesh& out, front_edge_handle e1, front_edge_handle e0, uint side, vec3* p0, vec3* p1, vec3* p2) {
    if (side == LEFT_EDGE) {
        out.edge_verts(out.get_edge(e1), p0, p1);
        *p2 = out.position(out.get_edge(e0, 1));
    } 
    if (side == RIGHT_EDGE) {
        *p0 = out.position(out.get_edge(e0));
        out.edge_verts(out.get_edge(e1), p1, p2);
    }
}

constexpr real SEAM_ANGLE = 1.0 / 6.0 * PI;

void QMorph::update_side_edge(QMorphFront& front, front_edge_handle e1, front_edge_handle e0, vec3 normal, uint side, bool push, bool delay) {
    front.remove(e1);
    //draw_front(debug, out, *this, front);
    //suspend_execution(debug);

    vec3 p0, p1, p2;
    get_verts(out, e1, e0, side, &p0, &p1, &p2);

    QMorphFront::Edge& side_edge = front.edges[e1];
    QMorphFront::Edge& edge = front.edges[e0];

    vertex_handle v;

    if (side == LEFT_EDGE) {
        side_edge.right = e0;
        edge.left = e1;
        
        v = out.indices[out.get_edge(e0)];
        edge.state &= ~LEFT_EDGE;
        side_edge.state &= ~RIGHT_EDGE;
    }
    else {
        side_edge.left = e0;
        edge.right = e1;

        v = out.indices[out.get_edge(e1)];
        edge.state &= ~RIGHT_EDGE;
        side_edge.state &= ~LEFT_EDGE;
    }

    real angle = vec_dir_angle(p0 - p1, p2 - p1, normal);        
    edge_handle dir_edge = side == LEFT_EDGE ? out.get_edge(e0) : out.get_edge(e1);
    if (fabs(angle - 0.5*PI) > 0.1 * PI && angle < 0.9*PI) {
        vec3 bitangent = normalize(cross(normal, p2 - p0));

        vec3 new_pos = (p0 + p2)/2.0f - bitangent* length(p2-p0) * 0.5f;
        draw_point(debug, new_pos, RED_DEBUG_COLOR);
        suspend_execution(debug);
        out.move_vert(dir_edge, new_pos*0.5 + p1*0.5);
        p1 = out.position(dir_edge);
    }

    else if ((angle < 1.0 / 4.0 * PI) || (angle > 0.9*PI && angle < 1.4*PI) ) {
        float factor = 1.0;

        float l0 = length(p0 - p1);
        float l1 = length(p1 - p2);

        float w0 = l0 / mesh_size;
        float w1 = l1 / mesh_size;

        vec3 new_pos = (p0 + p2) / 2.0f;// (p0 * w0 + p2 * w1) / (w0 + w1);
        

        out.move_vert(dir_edge, new_pos);
        p1 = out.position(dir_edge);
    }
    angle = vec_dir_angle(p0 - p1, p2 - p1, normal);

    if (angle < 0.75*PI) {
        edge.state |= side;
        side_edge.state |= ~side & 3;
    }

    edge.state &= ~SEAM;
    side_edge.state &= ~SEAM;

    if (angle < SEAM_ANGLE) {
        edge.state |= SEAM;
        side_edge.state |= SEAM;
    }

    delay = true;
    if (!delay) front.push(e1);
    if (push) {
        edge.state = front.gen(edge.state, delay);
        front.push(e0);
    }
    //front.edges[e1].state = front.gen(edge.state, true);
    if (delay) front.push(e1);

    if (angle == 0.0f) {
        suspend_and_reset_execution(debug);
        clear_debug_stack(debug);
        suspend_and_reset_execution(debug);
        draw_front(debug, out, *this, front);
        suspend_execution(debug);
        draw_mesh(debug, out);
        draw_edge(debug, out, out.get_edge(e1), GREEN_DEBUG_COLOR);
        draw_edge(debug, out, out.get_edge(e0), BLUE_DEBUG_COLOR);
        suspend_execution(debug);
    }

    //clear_debug_stack(debug);
    //draw_front(debug, out, *this, front);
    //suspend_execution(debug);
}

void remove_enclosed_tris(QMorph& q, slice<edge_handle> cavity_edges) {
    CFDDebugRenderer& debug = q.debug;
    SurfaceTriMesh& out = q.out;
    auto& stack = q.stack;
    
    out.dealloc_tri(cavity_edges[0]);
    stack.append(out.TRI(cavity_edges[0]));
    while (stack.length > 0) {
        tri_handle current = stack.pop();
        draw_triangle(debug, out, current, RED_DEBUG_COLOR);
        //suspend_execution(debug);

        for (uint i = 0; i < 3; i++) {
            edge_handle edge = current + i;
            tri_handle neigh = out.TRI(out.edges[edge]);
            if (out.is_deallocated(neigh)) continue;

            bool inside = true;
            for (uint j = 0; j < cavity_edges.length; j++) {
                if (edge == cavity_edges[j] || edge == out.edges[j]) { //todo redundant to check both
                    inside = false;
                    break;
                }
            }

            if (inside) {
                out.remove_edge(edge);
                out.dealloc_tri(neigh);
                stack.append(neigh);
            }
        }
    }

}

void QMorph::seam(front_edge_handle front_edge) {
    QMorphFront::Edge& edge = front.edges[front_edge];

    clear_debug_stack(debug);
    draw_front(debug, out, *this, front);
    draw_edge(debug, out, out.get_edge(front_edge), BLUE_DEBUG_COLOR);
    suspend_execution(debug);

    bool left = edge.state & LEFT_EDGE;

    front_edge_handle start, end;
    if (left) {
        end = edge.left; //
        start = out.get_opp(front_edge); 
    }
    else {
        end = out.get_opp(edge.right);
        start = front_edge;
    }
    
    stable_edge_handle top = recover_edge(out.get_edge(start), out.get_edge(end)); 

    vertex_handle v0 = out.indices[out.get_edge(start)];    
    vertex_handle v1 = out.indices[out.get_edge(end)];

    /*
    {
        front.edges[top]

        if (left) {
            vec3 normal = out.triangle_normal(out.get_tri(edge.right));
            update_side_edge(front, front.edges[edge.left].left, top, normal, LEFT_EDGE, false);
            update_side_edge(front, edge.right, top, normal, RIGHT_EDGE, true);
            front.dealloc(edge.left);
        }
        else {
            vec3 normal = out.triangle_normal(out.get_tri(edge.left));
            update_side_edge(front, front.edges[edge.right].right, top, normal, RIGHT_EDGE, false);
            update_side_edge(front, edge.left, top, normal, LEFT_EDGE, true);
            front.dealloc(edge.right);
        }


        return;
    }
    */

    suspend_execution(debug);
    draw_mesh(debug, out);
    draw_edge(debug, out, out.get_edge(top), RED_DEBUG_COLOR);
    suspend_execution(debug);  

    draw_point(debug, out.positions[v0.id], GREEN_DEBUG_COLOR);
    draw_point(debug, out.positions[v1.id], BLUE_DEBUG_COLOR);
    

    draw_mesh(debug, out);

    auto ccw = out.ccw(out.get_edge(end));    

    //Remove inner triangles
    
    edge_handle cavity_edges[4]; //edges belonging to cavity
    cavity_edges[0] = out.get_edge(front_edge);
    if (left) {
        cavity_edges[1] = out.get_edge(edge.left);
        cavity_edges[2] = out.get_opp_edge(top, 1);
    }
    else {
        cavity_edges[1] = out.get_edge(edge.right);
        cavity_edges[2] = out.get_edge(top, 1);
    }
    cavity_edges[3] = out.next_edge(cavity_edges[2]);

    suspend_and_reset_execution(debug);

    vec4 colors[4] = {
        RED_DEBUG_COLOR,
        GREEN_DEBUG_COLOR,
        BLUE_DEBUG_COLOR,
        vec4(1,1,0,1)
    };

    stable_edge_handle stable[4];
    edge_handle cavity_outer_edges[4]; //edges outside of cavity
    for (uint i = 0; i < 4; i++) {
        cavity_outer_edges[i] = out.edges[cavity_edges[i]];
        stable[i] = out.get_stable(cavity_edges[i]);
        draw_triangle(debug, out, out.TRI(cavity_outer_edges[i]), colors[i]);
        suspend_execution(debug);
    }

    remove_enclosed_tris(*this, { cavity_edges, 4 });

    //Merge vertices at mid
    vec3 mid = (out.positions[v0.id] + out.positions[v1.id]) / 2;    
    
    for (edge_handle edge : ccw) {
        draw_edge(debug, out, edge, RED_DEBUG_COLOR);
        out.indices[edge] = v0;
        out.indices[out.next_edge(out.edges[edge])] = v0;
    };    

    out.link_edge(out.edges[cavity_edges[0]], out.edges[cavity_edges[1]]);
    out.link_edge(out.edges[cavity_edges[2]], out.edges[cavity_edges[3]]);

    for (uint i = 0; i < 4; i++) out.map_stable_edge(out.edges[cavity_outer_edges[i]], stable[i]);

    out.positions[v0.id] = mid;
    //move_vert(out, debug, out.get_edge(start), mid);    
    
    draw_point(debug, mid, RED_DEBUG_COLOR);
   
    clear_debug_stack(debug);
    draw_mesh(debug, out);
    suspend_execution(debug);
    
    if (left) {        
        vec3 normal = out.triangle_normal(out.get_tri(edge.right));
        update_side_edge(front, front.edges[edge.left].left, edge.right, normal, LEFT_EDGE, false);
        front.dealloc(edge.left);
    }
    else {
        vec3 normal = out.triangle_normal(out.get_tri(edge.left));
        update_side_edge(front, front.edges[edge.right].right, edge.left, normal, RIGHT_EDGE, false);
        front.dealloc(edge.right);
    }

    clear_debug_stack(debug);
    draw_mesh(debug, out);
    suspend_execution(debug);
    clear_debug_stack(debug);
    draw_front(debug, out, *this, front);
    suspend_execution(debug);
}

stable_edge_handle form_side_edge(QMorph& q, tri_handle tri, edge_handle start, edge_handle other, vec3 normal, vec3 bitangent,  real mesh_size) {
    SurfaceTriMesh& out = q.out;
    CFDDebugRenderer& debug = q.debug;

    vertex_handle v_edge_other = out.indices[other];

    bitangent = normalize(bitangent);

    vec3 p0 = out.position(start);
    vec3 p1 = p0 + bitangent * mesh_size;

#if 1
    float clear = 1.5;
    //if (q.find_tri(tri, p0 + bitangent * mesh_size * clear)) {
        tri = q.find_tri(tri, p1);

        if (tri) {
            stable_edge_handle edge0 = out.get_stable(start);
            edge_handle edge1 = q.insert_into_triangle(tri, p1, false);
            return q.recover_edge(edge1, out.get_edge(edge0));
        }
    //}
#endif

    auto ccw = out.ccw(start);

    edge_handle best_edge = 0;
    real smallest_angle = FLT_MAX;

    /*suspend_execution(debug);
    draw_mesh(debug, out);
    draw_point(debug, p0, RED_DEBUG_COLOR);
    draw_line(debug, p0, p0 + bitangent, BLUE_DEBUG_COLOR);
    suspend_execution(debug);
    //clear_debug_stack(debug);*/

    for (uint i = 0; i < ccw.length; i++) {
        edge_handle edge = ccw[i];
        vec3 p1 = out.position(out.edges[edge]);

        real angle = vec_sign_angle(bitangent, p1 - p0, normal);

        draw_edge(debug, out, edge, color_map(fabs(angle), 0, PI));

        if (fabs(angle) < fabs(smallest_angle)) {
            best_edge = edge;
            smallest_angle = angle;
        }
    }

    bool forms_triangle = out.indices[out.next_edge(best_edge)].id == v_edge_other.id;

    const real threshold = 0.1 * PI;
    
    draw_edge(debug, out, best_edge, RED_DEBUG_COLOR);
    //suspend_execution(debug);

    //
    if (!forms_triangle && fabs(smallest_angle) < threshold) {
        //p0 --> p1
        //p1 --> p0
        //move_vert(out, debug, TRI_NEXT_EDGE(best_edge), p1);
        return out.get_opp(out.get_stable(best_edge)); 
    }

    edge_handle flip_edge = smallest_angle > 0 ? out.next_edge(best_edge) : out.next_edge(out.edges[best_edge], 2);
    draw_edge(debug, out, flip_edge, RED_DEBUG_COLOR);

    bool on_front = out.is_boundary(flip_edge);
    if (on_front) {
        //move_vert(out, debug, TRI_NEXT_EDGE(best_edge), p1);
        return out.get_opp(out.get_stable(best_edge));
    }

    vec3 p[4];
    out.diamond_verts(flip_edge, p);
   

    vec3 p13 = p[3] - p[1];

    float l = length(p13);

    float angle = vec_angle(bitangent, p13);

    stable_edge_handle result;

    if (l > sqrt(2) * mesh_size || angle > threshold || forms_triangle) {
        vec3 normal = out.triangle_normal(out.TRI(flip_edge));
        
        draw_line(debug, p[0], p[2], GREEN_DEBUG_COLOR);
        draw_line(debug, p0, p0 + bitangent, BLUE_DEBUG_COLOR);
        //suspend_execution(debug);
        
        vec3 inter;
        assert(edge_edge_intersect(normal, p[0], p[2], p0, p0 + bitangent, &inter));

        result = out.split_edge(flip_edge, inter);
        result = out.get_stable(out.get_edge(result, 2));
        result = out.get_opp(result);
    }
    else {
        //suspend_execution(debug);
        result = out.get_stable(out.flip_edge(flip_edge));
        /*clear_debug_stack(debug);
        draw_mesh(debug, out);
        draw_edge(debug, out, out.get_edge(result), RED_DEBUG_COLOR);
        suspend_execution(debug);
        */
    }

    //move_vert(out, debug, out.get_edge(result), p1);
    
    //draw_mesh(debug, out);    
    //draw_edge(debug, out, out.get_edge(result), RED_DEBUG_COLOR);

    return result;
}

/*
void form_triangle(QMorph& q, stable_edge_handle front_edge, stable_edge_handle top_edge, ) {
    SurfaceTriMesh& out = q.out;
    QMorphFront& front = q.front;

    QMorphFront::Edge& edge = front.edges[front_edge];

    bool left = edge.state & LEFT_EDGE;
    bool right = edge.state & RIGHT_EDGE;

    QMorphFront::Edge& top = front.edges[top_edge];

    vec3 normal = out.triangle_normal(out.get_tri(front_edge));
    
    if (left) {
        top.tm1 = edge.tm1;
        top.tm0 = front.edges[edge.left].tm0;

        q.update_side_edge(front, edge.right, top_edge, normal, LEFT_EDGE, false);
        q.update_side_edge(front, front.edges[edge.left].left, top_edge, normal, RIGHT_EDGE, true);
        front.dealloc(edge.left);
    }
    if (right) {
        top.tm0 = edge.tm0;
        top.tm1 = front.edges[edge.right].tm1;

        q.update_side_edge(front, edge.left, top_edge, normal, RIGHT_EDGE, false);
        q.update_side_edge(front, front.edges[edge.right].right, top_edge, normal, LEFT_EDGE, true);
        front.dealloc(edge.right);
    }

    front.dealloc(front_edge);
}
*/

void form_wedge(QMorph& q, stable_edge_handle edge, stable_edge_handle connect_with_edge, vec3 normal, vec3 bitangent, float l, uint side) {
    SurfaceTriMesh& out = q.out;
    QMorphFront& front = q.front;
    CFDDebugRenderer& debug = q.debug;

    QMorphFront::Edge& front_edge = front.edges[edge];

    clear_debug_stack(debug);
    draw_front(debug, out, q, front);
    suspend_execution(debug);
    
    bool left = side == LEFT_EDGE;

    edge_handle dir_edge = left ? out.get_edge(edge) : out.get_opp_edge(edge);

    vec3 p0 = out.position(dir_edge);

    bool connect = connect_with_edge.id != 0;

    stable_edge_handle new_edge;
    if (connect) new_edge = left ? connect_with_edge : out.get_opp(connect_with_edge);
    else new_edge = form_side_edge(q, out.get_tri(edge), dir_edge, 0, normal, bitangent, l);
    
    stable_edge_handle top;

    draw_mesh(debug, out);
    draw_line(debug, p0, p0 + bitangent, RED_DEBUG_COLOR);
    draw_edge(debug, out, out.get_edge(new_edge), BLUE_DEBUG_COLOR);
    suspend_execution(debug);

    vec3 p1 = out.position(out.get_edge(new_edge));
    vec3 dummy = p1;
    tri_handle tri = q.in.project(debug, front_edge.tm0, &dummy, nullptr);
    
    vec3 mid;

    if (left) {
        top = q.recover_edge(out.get_edge(new_edge), out.get_opp_edge(edge));
        clear_debug_stack(debug);
        draw_mesh(debug, out);
        suspend_execution(debug);
        
        new_edge = out.get_opp(new_edge);
        
        front.edges[top].tm0 = tri;
        front.edges[top].tm1 = front_edge.tm1;
       
        if (!connect) {
            front.edges[new_edge].tm0 = front_edge.tm0;
            front.edges[new_edge].tm1 = tri;
        }

        if (connect) q.update_side_edge(front, front.edges[connect_with_edge].left, top, normal, LEFT_EDGE, false);
        else q.update_side_edge(front, top, new_edge, normal, RIGHT_EDGE, false);
        
        q.update_side_edge(front, front_edge.right, top, normal, RIGHT_EDGE, true);
        if (!connect) q.update_side_edge(front, front_edge.left, new_edge, normal, LEFT_EDGE, true);
        
        mid = (out.position(out.get_edge(top)) + out.position(out.get_edge(front_edge.right, 1))) / 2.0f;
    }
    else {
        top = q.recover_edge(out.get_edge(edge), out.get_edge(new_edge));
        front.edges[top].tm0 = front_edge.tm0;
        front.edges[top].tm1 = tri;
        if (!connect) {
            front.edges[new_edge].tm0 = tri;
            front.edges[new_edge].tm1 = front_edge.tm1;
        }

        if (connect) q.update_side_edge(front, front.edges[connect_with_edge].right, top, normal, RIGHT_EDGE, false);
        else q.update_side_edge(front, top, new_edge, normal, LEFT_EDGE, false);

        q.update_side_edge(front, front_edge.left, top, normal, LEFT_EDGE, true);
        
        if (!connect) q.update_side_edge(front, front_edge.right, new_edge, normal, RIGHT_EDGE, true);

        mid = (out.position(out.get_edge(top, 1)) + out.position(out.get_edge(front_edge.left))) / 2.0f;
    }

    out.flip_bad_edges(out.get_edge(top));
    out.move_vert(left ? out.get_opp_edge(top) : out.get_edge(top), mid);
    
    if (connect) front.dealloc(connect_with_edge);
    front.dealloc(edge);
    
    suspend_execution(debug);
    clear_debug_stack(debug);
    draw_mesh(debug, out);
    draw_point(debug, p0, RED_DEBUG_COLOR);
    draw_point(debug, p1, BLUE_DEBUG_COLOR);
    suspend_execution(debug);
    clear_debug_stack(debug);
    draw_front(debug, out, q, front);
    suspend_execution(debug);
}

/*
if (left) {
    vec3 p2 = out.position(out.get_edge(edge.left));
    real r = length(p2 - p1) / 2.0f;
    vec3 mid = (p2 + p1) / 2.0;

    vec3 p3 = mid + r*normalize(mid - p0);
    bitangent1 = p3 - p1;
    l1 = length(bitangent1);
    bitangent1 /= l1;
}
if (right) {
    vec3 p2 = out.position(out.get_opp_edge(edge.right));
    real r = length(p2 - p0) / 2.0f;
    vec3 mid = (p2 + p0) / 2.0f;

    vec3 p3 = mid + r * normalize(mid - p1);
    bitangent0 = p3 - p0;
    l0 = length(bitangent0);
    bitangent0 /= l0;
}
*/

void advance_front(QMorph& q, stable_edge_handle front_edge) {
    CFDDebugRenderer& debug = q.debug;
    QMorphFront& front = q.front;
    SurfaceTriMesh& out = q.out;
    SurfaceTriMesh& in = q.in;
    SurfaceCrossField& cross_field = q.cross_field;
    real mesh_size = q.mesh_size;

    QMorphFront::Edge& edge = front.edges[front_edge];

    //Check if front forms closed loop
    front_edge_handle loop_edge;
    bool loop3, loop4;
    {
        loop_edge = front.edges[edge.right].right;
        loop3 = loop_edge == edge.left;
        loop4 = edge.left == front.edges[loop_edge].right;

        if (loop3) {
            front.dealloc(edge.right);
            front.dealloc(edge.left);
            return;
        }

        if (loop4) {
            front.dealloc(loop_edge);
            front.dealloc(edge.right);
            front.dealloc(edge.left);
            return;
        }
    }

    if (edge.state & SEAM) {
        q.seam(front_edge);
        return;
    }

    clear_debug_stack(debug);

    bool left = edge.state & LEFT_EDGE;
    bool right = edge.state & RIGHT_EDGE;

    edge_handle e0 = out.get_edge(front_edge); //find_edge(edge.tr, edge.edge_center, edge.edge_dir);
    edge_handle e1 = out.edges[e0]; //find_edge(edge.tr, edge.edge_center, edge.edge_dir);
    //draw_edge(debug, out, e0, BLUE_DEBUG_COLOR);

    vec3 p0 = out.position(e0);
    vec3 p1 = out.position(out.next_edge(e0));

    vec3 normal0 = out.triangle_normal(out.TRI(e0));
    vec3 normal = normal0;

    /*if (normal0.x < 0.9) {
        suspend_and_reset_execution(debug);
        draw_mesh(debug, out);
        draw_front(debug, out, *this, front);
        draw_triangle(debug, out, out.TRI(e0), RED_DEBUG_COLOR);
        draw_line(debug, p0, p0 + normal, RED_DEBUG_COLOR);
        suspend_execution(debug);
    }*/

    vec3 tangent = p1 - p0;
    real edge_length = length(tangent);
    vec3 bitangent_edge = normalize(cross(normal0, tangent));

    auto side_edge_dir = [&](tri_handle tri, front_edge_handle e1, front_edge_handle e2, uint side, vec3* bitangent) {
        vec3 bitangent_side;
        vec3 p0, p1, p2;
        get_verts(out, e1, e2, side, &p0, &p1, &p2);

        real angle = vec_dir_angle(p0 - p1, p2 - p1, normal);

        *bitangent = bitangent_edge;
        if (angle < PI) *bitangent = normalize(cross(normal, p2 - p0));

        //return *bitangent;
        return cross_field.cross_vector(tri, p1, *bitangent);
    };

    vec3 edge_bitangent0, edge_bitangent1;

    vec3 bitangent0 = side_edge_dir(edge.tm0, edge.left, front_edge, LEFT_EDGE, &edge_bitangent0);
    vec3 bitangent1 = side_edge_dir(edge.tm1, edge.right, front_edge, RIGHT_EDGE, &edge_bitangent1);
    float l0 = mesh_size; // fminf(mesh_size, edge_length); // / dot(bitangent0, bitangent_edge);
    float l1 = mesh_size; // fminf(mesh_size, edge_length); /// dot(bitangent1, bitangent_edge); 

    vec3 p2, p3;
    if (left) p2 = out.position(out.get_edge(edge.left));
    else p2 = p0 + bitangent0 * l0;

    if (right) p3 = out.position(out.get_opp_edge(edge.right));
    else p3 = p1 + bitangent1 * l1;

    real size_diff = length(p3 - p2) / mesh_size;

    enum Wedge {
        None,
        Triangle,
        TriangleExisting,
        Diamond
    };

    if (size_diff < 0.6) {
        bitangent0 = edge_bitangent0;
        bitangent1 = edge_bitangent1;
    }

    bool wedge = size_diff > 1.8;
    Wedge left_wedge = Wedge::None;
    Wedge right_wedge = Wedge::None;

    vec3 bitangent_wedge0, bitangent_wedge1;

    real left_angle = vec_dir_angle(p2 - p0, p1 - p0, normal);
    real right_angle = vec_dir_angle(p0 - p1, p3 - p1, normal);


    constexpr float angle = PI / 8.0f;

    auto would_cause_seam = [angle](real corner_angle) {
        //return true;
        return corner_angle - 2 * angle < 1.0 / 3.0f * PI;
    };

    if (wedge) {
        if (left_angle > right_angle) {
            bitangent0 = edge_bitangent0;
            if (left) {
                //bitangent0 = glm::rotate(glm::vec3(bitangent0), -angle*0.3f, glm::vec3(normal));

                left_wedge = would_cause_seam(left_angle) ? Wedge::TriangleExisting : Wedge::Diamond;
                left = false;
            }
            else {
                left_wedge = would_cause_seam(left_angle) ? Wedge::Triangle : Wedge::Diamond;
                bitangent0 = glm::rotate(glm::vec3(bitangent0), -angle, glm::vec3(normal));
                bitangent_wedge0 = glm::rotate(glm::vec3(bitangent0), angle, glm::vec3(normal));
            }

        }
        else {
            bitangent1 = edge_bitangent1;
            if (right) {
                right = false;
                right_wedge = would_cause_seam(right_angle) ? Wedge::TriangleExisting : Wedge::Diamond;
                //bitangent1 = glm::rotate(glm::vec3(bitangent1), angle * 0.3f, glm::vec3(normal));
            }
            else {
                right_wedge = would_cause_seam(right_angle) ? Wedge::Triangle : Wedge::Diamond;
                bitangent1 = glm::rotate(glm::vec3(bitangent1), angle, glm::vec3(normal));
                bitangent_wedge1 = glm::rotate(glm::vec3(bitangent1), -angle, glm::vec3(normal));
            }
        }
    }

    float factor = 1.0f;

    tri_handle tr2 = 0, tr3 = 0;

    stable_edge_handle e2, e3;

    tri_handle tri = out.TRI(out.get_edge(front_edge));
    if (!left) {
        e2 = form_side_edge(q, tri, out.get_edge(front_edge), right ? out.get_opp_edge(edge.right) : 0, normal, bitangent0, l0);
        p2 = out.position(out.get_edge(e2));
        e2 = out.get_opp(e2); //p0 --> p2
    }
    else {
        e2 = edge.left; //  p2 ----> p0
        e2 = out.get_opp(e2); //  p0 ----> p2
    }

    if (!right) {
        e3 = form_side_edge(q, tri, out.get_opp_edge(front_edge), left ? out.get_edge(edge.left) : 0, normal, bitangent1, l1); //p3 --> p1
        p3 = out.position(out.get_edge(e3)); //p3 --> p1
        draw_point(debug, p3, BLUE_DEBUG_COLOR);
    }
    else {
        e3 = edge.right; //  x ---> r
        e3 = out.get_stable(out.edges[out.get_edge(e3)]); //r --> x
    }

    edge_handle dir_e0 = out.edges[out.get_edge(e2)];
    edge_handle dir_e1 = out.get_edge(e3);

    bool forms_triangle = out.indices[dir_e0].id == out.indices[dir_e1].id;
    /*if (forms_triangle) {

        draw_mesh(debug, out);
        suspend_and_reset_execution(debug);
        form_triangle(*this, front_edge, left ? e3 : e2);
        continue;
    }*/

    if (edge.tm0 == 0 || edge.tm1 == 0) {
        draw_edge(debug, out, out.get_edge(front_edge), RED_DEBUG_COLOR);
        suspend_execution(debug);
    }

    tri_handle tm2, tm3;
    if (!left) {
        tm2 = in.project(debug, edge.tm0, &p2, nullptr);
        if (!tm2) {
            suspend_execution(debug);
            printf("BADD!!!!!\n");
        }
    }
    else tm2 = front.edges[edge.left].tm0;

    if (!right) {
        tm3 = in.project(debug, edge.tm1, &p3, nullptr);
        if (!tm3) {
            suspend_execution(debug);
            printf("BADD!!!!\n");
        }
    }
    else tm3 = front.edges[edge.right].tm1;

    array<6, tri_handle> flip;

    //draw_triangle(debug, out, tr2, BLUE_DEBUG_COLOR);
    //draw_triangle(debug, out, tr3, BLUE_DEBUG_COLOR);
    //suspend_execution(debug);

    front_edge_handle top_edge;
    if (!forms_triangle) {
        top_edge = q.recover_edge(dir_e0, dir_e1);
        out.mark_boundary(top_edge);
        out.mark_boundary(e2);
        out.mark_boundary(e3);
        out.flip_bad_edges(out.get_tri(top_edge));
    }

    if (!left) {
        QMorphFront::Edge& left_side = front.edges[e2];
        left_side.tm0 = edge.tm0;
        left_side.tm1 = tm2;

        out.flip_bad_edges(out.get_tri(e2));
        out.move_vert(out.get_opp_edge(e2), p0 + l0 * bitangent0);
        q.update_side_edge(front, edge.left, e2, normal, LEFT_EDGE, true);
    }

    if (!right) {
        QMorphFront::Edge& right_side = front.edges[e3];
        right_side.tm0 = tm3;
        right_side.tm1 = edge.tm1;

        out.flip_bad_edges(out.get_tri(e3));
        out.move_vert(out.get_edge(e3), p1 + l1 * bitangent1);
        q.update_side_edge(front, edge.right, e3, normal, RIGHT_EDGE, true);
    }

    if (e3.id == 0 || e2.id == 0) {
        suspend_execution(debug);
        draw_mesh(debug, out);
        suspend_execution(debug);
    }

    //p2 --> p0, p3 --> p1
    //draw_mesh(debug, out);
    //suspend_execution(debug);

    bool top_is_front = false;
    QMorphFront::Edge* top = nullptr;

    if (!forms_triangle) {
        top_is_front = front.contains(out.get_opp(top_edge));
        assert(!front.contains(top_edge));

        if (top_is_front) {
            top_edge = out.get_opp(top_edge);
            front.remove(top_edge);
        }

        top = &front.edges[top_edge];

        top->state = 0;
        top->tm0 = tm2;
        top->tm1 = tm3;
    }

    //todo refactor logic
    if (!left) {
        if (top_is_front) {
            q.update_side_edge(front, top->right, e2, normal, RIGHT_EDGE, false);
        }
        else if (forms_triangle) {
            front.edges[e2].right = e3;
            front.edges[e3].left = e2;
        }
        else {
            top->left = e2;
            front.edges[e2].right = top_edge;
        }
    }
    else {
        q.update_side_edge(front, front.edges[edge.left].left, forms_triangle ? e3 : top_edge, normal, LEFT_EDGE, false, !wedge);
        front.dealloc(edge.left);
    }

    if (!right) {
        if (top_is_front) {
            q.update_side_edge(front, top->left, e3, normal, LEFT_EDGE, false);
        }
        else if (forms_triangle) {
            front.edges[e2].right = e3;
            front.edges[e3].left = e2;
        }
        else {
            top->right = e3;
            front.edges[e3].left = top_edge;
        }
    }
    else {
        QMorphFront::Edge& right = front.edges[edge.right];
        q.update_side_edge(front, right.right, forms_triangle ? e2 : top_edge, normal, RIGHT_EDGE, false, !wedge);
        front.dealloc(edge.right);
    }

    if (!forms_triangle) {
        bool next_gen = state_to_priority(edge.state) < 2; //delay untill next iteration
        top->state = front.gen(top->state, next_gen);

        if (!top_is_front) front.push(top_edge);
        else front.dealloc(top_edge);
    }
    else {
        return;
    }

    if (false && wedge) {
        draw_mesh(debug, out);
        suspend_and_reset_execution(debug);
        clear_debug_stack(debug);
        draw_front(debug, out, q, front);
        suspend_execution(debug);
    }

    if (left_wedge == Wedge::Triangle || left_wedge == Wedge::TriangleExisting) {
        form_wedge(q, e2, left_wedge == Wedge::TriangleExisting ? edge.left : stable_edge_handle{}, normal, bitangent_wedge0, l0, LEFT_EDGE);
    }
    if (right_wedge == Wedge::Triangle || right_wedge == Wedge::TriangleExisting) {
        form_wedge(q, e3, right_wedge == Wedge::TriangleExisting ? edge.right : stable_edge_handle{}, normal, bitangent_wedge1, l1, RIGHT_EDGE);
    }
    if (left_wedge == Wedge::Diamond) {
        front.remove(front.edges[e2].left);
        advance_front(q, front.edges[e2].left);
    }
    if (right_wedge == Wedge::Diamond) {
        front.remove(front.edges[e3].right);
        advance_front(q, front.edges[e3].right);
    }

    //form quad
    if (top_is_front) top_edge = out.get_opp(top_edge);

    //ccw order
    edge_handle cavity_outer_edges[4] = { out.get_opp_edge(front_edge), out.get_edge(e3), out.get_edge(top_edge), out.get_edge(e2), };
    edge_handle cavity_inner_edges[4];
    stable_edge_handle cavity_stable[4];
    for (uint i = 0; i < 4; i++) {
        cavity_inner_edges[i] = out.edges[cavity_outer_edges[i]];
        cavity_stable[i] = out.get_stable(cavity_inner_edges[i]);
    }
    remove_enclosed_tris(q, { cavity_inner_edges, 4 });

    tri_handle quad = out.alloc_tri();

    //ccw vert orientation
    out.edge_verts(out.get_edge(front_edge), &out.indices[quad + 0], &out.indices[quad + 1]);
    out.edge_verts(out.get_edge(top_edge), &out.indices[quad + 3], &out.indices[quad + 2]);

    out.flags[quad / 4] |= IS_QUAD;
    for (uint i = 0; i < 4; i++) {
        //draw_triangle(debug, out, out.TRI(cavity_outer_edges[i]), GREEN_DEBUG_COLOR);
        //draw_edge(debug, out, quad + i, RED_DEBUG_COLOR);
        //suspend_execution(debug);
        out.edges[quad + i] = cavity_outer_edges[i];
        out.edges[cavity_outer_edges[i]] = quad + i;
        out.map_stable_edge(quad + i, cavity_stable[i]);
        out.mark_boundary(cavity_stable[i]);
    }
}

void QMorph::propagate(QMorphFront& new_front) {
    front = std::move(new_front);
    
    suspend_execution(debug);
    clear_debug_stack(debug);
    draw_front(debug, out, *this, front);
    suspend_execution(debug);

    uint count = 0;
    
    while (true) {
        front_edge_handle front_edge = front.pop();
        if (!front_edge.id) break;

        advance_front(*this, front_edge);

#if 0
        //if (count++ % 20 == 0) {
            suspend_execution(debug);
            clear_debug_stack(debug);
            draw_mesh(debug, out);
            suspend_execution(debug);
            clear_debug_stack(debug);
            draw_front(debug, out, *this, front);
            suspend_execution(debug);
        //}
#endif
        

    }

    clear_debug_stack(debug);
    memset(out.edge_flags, 0, sizeof(char) * out.stable_to_edge.length);
    out.smooth_mesh(5);
    draw_mesh(debug, out);

    /*for (tri_handle tri : out) {
        vec3 verts[3];
        out.triangle_verts(tri, verts);
        draw_triangle(debug, verts, vec4(1));
    }*/

    suspend_execution(debug);
}

void qmorph(SurfaceTriMesh& mesh, CFDDebugRenderer& debug, SurfaceCrossField& cross_field, slice<stable_edge_handle> edges) {
    suspend_execution(debug);
    
    QMorph qmorph{
        mesh, cross_field, debug, {nullptr, nullptr}, {}
    };

    qmorph.out.copy(mesh, true);

    qmorph.mesh_size = 0.1;
    qmorph.out.debug = &debug;

    auto fronts = qmorph.identify_edge_loops(edges);

    //qmorph.propagate(fronts[3]);
    //todo parallelize
    for (uint i = 0; i < fronts.length; i++) {
        auto& front = fronts[i];
        qmorph.propagate(front);
    }
}
