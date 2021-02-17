#include "core/math/vec4.h"
#include "core/math/vec3.h"
#include <glm/mat3x3.hpp>
#include "core/container/tvector.h"
#include "core/container/hash_map.h"
#define _USE_MATH_DEFINES
#include <math.h>

#include "graphics/culling/aabb.h"
#include "mesh.h"
#include "core/memory/linear_allocator.h"

#include "geo/predicates.h"
#include "mesh_generation/delaunay.h"

using tet_handle = uint;

struct CavityFace {
    tet_handle tet;
    vertex_handle verts[3];
};

struct TetFaces {
    tet_handle neighbor : 30;
    uint neighbor_face : 2;
};

struct Tet {
    tet_handle prev;
    tet_handle next;
};

struct VertexInfo {
    vec3 normal;
    float curvature; //+ concave, 0 flat, - convex
    uint count = 0;
};

const tet_handle NEIGH = ((1 << 30)-1) << 2;
const tet_handle NEIGH_FACE = (1<<2)-1;

struct Delaunay {
    //Delaunay
    CFDVolume& volume;
    vector<CFDVertex>& vertices;
    hash_map_base<TriangleFaceSet, tet_handle> shared_face;
    AABB aabb;
    uint max_shared_face;
    uint super_vert_offset;
    tvector<CavityFace> cavity;
    tvector<tet_handle> stack;
    tvector<tet_handle> free;
    tet_handle last;
    //tet_handle free_chain;

    //Mesh
    uint tet_count;
    //Tet* tets;
    vertex_handle* indices;
    tet_handle* faces;
    float* subdets;
    
    uint tet_alloc_count;

    //Advancing front
    //tet_handle active_faces;
    VertexInfo* active_verts;
    uint active_vert_offset;
    uint prev_active_vert_offset;
    tvector<vertex_handle> insertion_order;
    tvector<tet_handle> active_faces;
    uint grid_vert_begin;
    uint grid_vert_end;
};

float sq_distance(glm::vec3 a, glm::vec3 b) {
	glm::vec3 dist3 = a - b;
	return glm::dot(dist3, dist3);
}

struct Circumsphere {
    vec3 center;
    float radius;
};

bool point_inside(Circumsphere& sphere, vec3 point, float bias) {
    return length(point - sphere.center)+bias < sphere.radius;
}

real orient3d(vec3 a, vec3 b, vec3 c, vec3 d) {
    return orient3d(&a.x, &b.x, &c.x, &d.x);
}

Circumsphere circumsphere(vec3 p[4]) {
    vec3 u1 = p[0] - p[3];
    vec3 u2 = p[1] - p[3];
    vec3 u3 = p[2] - p[3];

    vec3 center = (sq_distance(p[0], p[3]) * cross(u2, u3)
        + sq_distance(p[1], p[3]) * cross(u3, u1)
        + sq_distance(p[2], p[3]) * cross(u1, u2))
        / (2 * dot(u1, cross(u2, u3)));
    center += p[3];
    float r = length(center - p[0]);
    return {center, r};
}

// hextreme
// see Perturbations and Vertex Removal in a 3D Delaunay Triangulation, O. Devillers & M. Teillaud
static inline real symbolicPerturbation (vertex_handle indices[5], vec3 i, vec3 j, vec3 k, vec3 l, vec3 m){
  
  const vec3* pt[5] = {&i,&j,&k,&l,&m};

  // Sort the five points such that their indices are in the increasing
  //   order. An optimized bubble sort algorithm is used, i.e., it has
  //   the worst case O(n^2) runtime, but it is usually much faster.
  int swaps = 0; // Record the total number of swaps.
  int n = 5;
  int count;
  do {
    count = 0;
    n = n - 1;
    for (int i = 0; i < n; i++) {
      if (indices[i].id > indices[i+1].id) {

        const vec3* swappt = pt[i];
        pt[i] = pt[i+1];
        pt[i+1] = swappt;

        vertex_handle sw = indices[i];
        indices[i] = indices[i+1];
        indices[i+1] = sw;
        count++;
      }
    }
    swaps += count;
  } while (count > 0); // Continue if some points are swapped.
  
  real oriA = orient3d(&pt[1]->x, &pt[2]->x, &pt[3]->x, &pt[4]->x);
  if (oriA != 0.0) {
    // Flip the sign if there are odd number of swaps.
    if ((swaps % 2) != 0) oriA = -oriA;
    return oriA;
  }
  
  real oriB = -orient3d(&pt[0]->x, &pt[2]->x, &pt[3]->x, &pt[4]->x);

  // Flip the sign if there are odd number of swaps.
  if ((swaps % 2) != 0) oriB = -oriB;
  return oriB;
  return 1.0;
}

#define NE_WARNING(mesg) fprintf(stderr, "Warning: %s\n", mesg);
#define NE_INFO(mesg) fprintf(stderr, "Info: %s\n", mesg);


real insphere(Delaunay& d, tet_handle curr, vertex_handle v) {
    float* subdet = d.subdets+curr;
    
#if 1
    vec3 p[5];
    for (uint i = 0; i < 4; i++) p[i] = d.vertices[d.indices[curr+i].id].position;
    p[4] = d.vertices[v.id].position;
    
    real det = insphere(&p[0].x, &p[1].x, &p[2].x, &p[3].x, &p[4].x);
    
    if (det == 0.0) {
        vertex_handle nn[5];
        memcpy_t(nn, d.indices + curr, 4);
        nn[4] = v;
        //NE_INFO("symbolic perturbation");
        det = symbolicPerturbation (nn,p[0],p[1],p[2],p[3],p[4]);
    }
    
    return det;
    
    
#else

    vec3 a = mesh[cell.vertices[0]].position;
    vec3 e = mesh[v].position;
    vec3 ae = e - a;

    if (cell.vertices[3].id == -1){
        real det = dot(ae,subdet);

        if(fabs(det) > o3dstaticfilter) return det;

        vec3 b = mesh.vertices[cell.vertices[1].id].position;
        vec3 c = mesh.vertices[cell.vertices[2].id].position;

        det = orient3d(a,b,c,e);

        if(det != 0.0) return det;

        // we never go here, except when point are aligned on boundary
        // NE_INFO("insphere using opposite vertex");
        CFDCell& opposite_cell = mesh[cell.faces[3].neighbor];
        vertex_handle opposite_v = opposite_cell.vertices[0];
        vec3 opposite_vertex = mesh[opposite_v].position;
        det = -insphere(&a.x,&b.x,&c.x,&opposite_vertex.x,&e.x);

        if (det == 0.0) {
          vertex_handle nn[5] = {cell.vertices[0],cell.vertices[1],cell.vertices[2],opposite_v,v};
          NE_INFO("symbolic perturbation on boundary");
          det = -symbolicPerturbation (nn, a,b,c,opposite_vertex,e);
          if (det == 0.0) NE_WARNING("Symbolic perturbation failed\n");
        }
        return det;
    }

    real aer = dot(ae,ae);
    real det = ae.x*subdet.x -ae.y*subdet.y+ae.z*subdet.z-aer*subdet.w;

    if (fabs(det) > ispstaticfilter) return det;

    vec3 b = mesh[cell.vertices[1]].position;
    vec3 c = mesh[cell.vertices[2]].position;
    vec3 d = mesh[cell.vertices[3]].position;

    det = insphere(&a.x,&b.x,&c.x,&d.x,&e.x);

    if (det == 0.0) {
        vertex_handle nn[5];
        memcpy_t(nn, cell.vertices, 4);
        nn[4] = v;
        NE_INFO("symbolic perturbation");
        det = symbolicPerturbation (nn, a,b,c,d,e);
        if (det == 0.0) NE_WARNING("total fail\n");
    }
    return det;//insphere(a,b,c,d,e);
#endif
}


/* compute sub-determinent of tet curTet */
void compute_subdet(Delaunay& d, tet_handle tet) {
    auto& indices = d.indices;
    auto& vertices = d.vertices;
    
    vec3 centroid;
    for (uint i = 0; i < 4; i++) {
        vec3 position = d.vertices[indices[tet+i].id].position;
        centroid += position / 4.0f;
    }

    vec4 ab;
    vec4 ac;

    float* subdet = d.subdets + tet;

    vec3 a = vertices[indices[tet+0].id].position;
    vec3 b = vertices[indices[tet+1].id].position;
    vec3 c = vertices[indices[tet+2].id].position;

    if(indices[tet+3].id != -1) {
        vec3 d = vertices[indices[tet+3].id].position;
        vec4 ad;

        uint i;
        for (i=0; i<3; i++) {
            ab[i]=b[i]-a[i]; //AB
            ac[i]=c[i]-a[i]; //AC
            ad[i]=d[i]-a[i]; //AD
        }

        ab.w = dot(ab,ab);
        ac.w = dot(ac,ac);
        ad.w = dot(ad,ad);
        
        real cd12 = ac.z*ad.y - ac.y*ad.z;
        real db12 = ad.z*ab.y - ad.y*ab.z;
        real bc12 = ab.z*ac.y - ab.y*ac.z;

        real cd30 = ac.x*ad.w - ac.w*ad.x;
        real db30 = ad.x*ab.w - ad.w*ab.x;
        real bc30 = ab.x*ac.w - ab.w*ac.x;

        // each subdet is a simple triple product
        subdet[0] = ab.w*cd12 + ac.w*db12 + ad.w*bc12;
        subdet[1] = ab.z*cd30 + ac.z*db30 + ad.z*bc30;
        subdet[2] = ab.y*cd30 + ac.y*db30 + ad.y*bc30;
        subdet[3] = ab.x*cd12 + ac.x*db12 + ad.x*bc12;

    // SubDet[3]=AD*(ACxAB) = 6 X volume of the tet !
    }
    else {
        uint i;
        for (i=0; i<3; i++) {
          ab[i]=b[i]-a[i]; //AB
          ac[i]=c[i]-a[i]; //AC
        }

        vec3 c = cross(ac, ab);
        *(vec4*)subdet = vec4(c,dot(c,c));
    }
}

float tetrahedron_skewness(vec3 positions[4]) {
	float e1 = length(positions[0] - positions[1]);
	float e2 = length(positions[0] - positions[1]);
	float e3 = length(positions[1] - positions[2]);
	float e4 = length(positions[2] - positions[0]);
	float e5 = length(positions[0] - positions[3]);
	float e6 = length(positions[1] - positions[3]);

	float al = (e1 + e2 + e3 + e4 + e5 + e6) / 6;
	float skewness = fabs(e1 - al) + fabs(e2 - al) + fabs(e3 - al) + fabs(e4 - al) + fabs(e5 - al) + fabs(e6 - al);
	skewness /= 6 * al;

	return skewness;
}

bool point_inside(Delaunay& d, tet_handle tet, vec3 pos) {
    for (uint i = 0; i < 4; i++) {
        vec3 verts[3];
        for (uint j = 0; j < 3; j++) {
            verts[j] = d.vertices[d.indices[tet+tetra_shape[i][j]].id].position;
        }
        
        vec3 n = triangle_normal(verts);
        vec3 p = d.vertices[d.indices[tet+tetra_shape[i][0]].id].position;
        float d = dot(n, pos - p);
        if (d > 3*FLT_EPSILON) {
            return false;
        }
    }

    return true;
}

vector<AABB> debug_box;

void clear_debug() {
    debug_box.clear();
}

void add_box_cell(CFDVolume& volume, vec3 p, vec3 s) {
    s /= 10;
    debug_box.append({p-s, p+s});
}

void draw_ray(CFDVolume& volume, vec3 orig, vec3 dir, float t) {
    uint n = 100;
    for (uint i = 0; i < n; i++) {
        vec3 p = orig + i * t / n * dir;
        add_box_cell(volume, p, 0.1);
    }
}

void draw_debug_boxes(CFDVolume& volume) {
    for (AABB& aabb : debug_box) {
        glm::vec3 verts[8];
        aabb.to_verts(verts);
        uint offset = volume.vertices.length;

        CFDCell cell{ CFDCell::HEXAHEDRON };
        for (uint i = 0; i < 8; i++) {
            volume.vertices.append({ verts[i] });
            cell.vertices[i] = { (int)(offset + i) };
        }

        volume.cells.append(cell);
    }
}

void assert_neighbors(CFDVolume& volume, cell_handle handle, TriangleFaceSet set) {
    return;
    CFDCell& cell = volume.cells[handle.id];
    bool is_neighbor = false;
    for (uint i = 0; i < 4; i++) {
        vertex_handle verts[3];
        for (uint j = 0; j < 3; j++) {
            verts[j] = cell.vertices[tetra_shape.faces[i].verts[j]];
        }

        if (set == verts) {
            is_neighbor = true;
            break;
        }
    }
    assert(is_neighbor);
}

vec3 compute_centroid(Delaunay& d, tet_handle tet) {
    vec3 centroid;
    for (uint i = 0; i < 4; i++) {
        centroid += d.vertices[d.indices[tet+i].id].position;
    }
    centroid /= 4;
    return centroid;
}

tet_handle find_enclosing_tet(Delaunay& d, vec3 pos, float min_dist = 0.0) {
    //Walk structure to get to enclosing triangle, starting from the last inserted
    tet_handle current = d.last;
    tet_handle prev = 0;

    //printf("==============\n");
    //printf("Starting at %i\n", current.id);
    
    //clear_debug();
    //add_box_cell(d.volume, pos, vec3(3));

    uint count = 0;
    loop: {
        uint offset = rand();
        
        for (uint it = 0; it < 4; it++) {
            uint i = (it + offset) % 4;
            tet_handle neighbor = d.faces[current + i] & NEIGH;
            if (!neighbor || neighbor == prev) continue;

            vec3 verts[3];
            for (uint j = 0; j < 3; j++) {
                verts[j] = d.vertices[d.indices[current + tetra_shape[i][j]].id].position;
                //add_box_cell(d.volume, verts[j], 0.1);
            }
            
            //vec3 n = triangle_normal(verts);

            float det =
            //dot(n, pos - verts[0]);
            -orient3d(verts[0], verts[1], verts[2], pos);
            if (det > 0) {
                //vec3 start = compute_centroid(d, current);
                //vec3 end = compute_centroid(d, neighbor);
                
                //draw_ray(d.volume, start, normalize(end-start), length(end-start));
                
                prev = current;
                current = neighbor;
                if (count++ > 1000) return 0;
                goto loop;
            }
        }
    };
    
    if (min_dist != 0.0f) {
        for (uint i = 0; i < 4; i++) {
            vec3 vert = d.vertices[d.indices[current+i].id].position;
            if (length(vert - pos) < min_dist) {
                return 1;
            }
        }
    }

    return current;
}

void dealloc_tet(Delaunay& d, tet_handle tet) {
    d.subdets[tet+3] = -1; //Mark for deletion and that it is already visited
    d.tet_alloc_count--;
    d.free.append(tet);
    
    /*
    for (uint i = 0; i < 4; i++) {
        uint idx = tet+i;
        Tet& tet = d.tets[idx];
        if (d.active_faces == idx) {
            printf("Removing last link %i: %i", idx, tet.next);
            d.active_faces = d.tets[idx].next;
            if (tet.next) d.tets[tet.next].prev = 0;
        }
        else if (tet.prev) {
            printf("Removing link %i: %i %i\n", idx, tet.prev, tet.next);
            d.tets[tet.prev].next = tet.next;
            assert(tet.next != tet.prev);
            if (tet.next) {
                d.tets[tet.next].prev = tet.prev;
                assert(d.tets[tet.next].next != d.tets[tet.next].prev);
            }
        }
    }*/
    
    //printf("Dealloc %i\n", tet);
    
    /*assert(tet != 0);
    assert(d.free_chain % 4 == 0);
    assert(tet % 4 == 0);
    d.tets[tet].next = d.free_chain;
    d.tets[tet].prev = 0;
    d.free_chain = tet;*/
}

int find_in_circum(Delaunay& d, vertex_handle v, float min_dist = 0.0f) {
    vec3 pos = d.vertices[v.id].position;
    //printf("Inserting %i,  (%f, %f, %f)\n", v.id, pos.x, pos.y, pos.z);
    tet_handle current = find_enclosing_tet(d, pos, min_dist);
    if (!current) {
        add_box_cell(d.volume, pos, 1);
        draw_debug_boxes(d.volume);
        printf("Could not find enclosing tet!\n");
        return false;
    }
    if (current == 1) return 2; //too close
    
    //if (!point_inside(d, current, pos)) return false;
    //assert(insphere(d, current, v) < 0);

    dealloc_tet(d, current);
    
    d.cavity.length = 0;
    d.stack.append(current);

    while (d.stack.length > 0) {
        tet_handle current = d.stack.pop();
        //Tet& cell = d.tets[current];
        //stack = cell.prev;

        //printf("Visiting %i\n", current);
        for (uint i = 0; i < 4; i++) {
            tet_handle neighbor = d.faces[current+i]&NEIGH;

            vertex_handle verts[3];
            for (uint j = 0; j < 3; j++) {
                verts[j] = d.indices[current + tetra_shape[i][2-j]];
                //Reverse orientation, as tetrashape will reverse orientation when extruding
                //Reverse-reverse -> no face flip -> as face should point outwards
            }
            
            if (neighbor) {
                bool visited = d.subdets[neighbor+3] == -1;
                bool inside = visited;
                if (!visited) {
                    float value = insphere(d, neighbor, v);
                    //printf("Insphere %f\n", value);
                    inside |= value < 0;
                }
                
                if (!visited && inside) {
                    //printf("Neighbor %i: adding to stack\n", neighbor.id);
                    dealloc_tet(d, neighbor);
                    d.stack.append(neighbor);
                }

                //printf("Face %i, neighbor %i, %s\n", i, neighbor.id, inside ? "not unique" : "unique");
                if (!inside) { //face is unique
                    //printf("Neighbor %i: Unique face %i\n", neighbor.id, i);
                    //assert_neighbors(volume, neighbor, verts);
                    d.cavity.append({ d.faces[current + i], verts[0], verts[1], verts[2] });
                }

                //if (visited) printf("Neighbor %i: already visited %i\n", neighbor.id, i);
            }
            else {
                //printf("Outside\n");
                d.cavity.append({ 0, verts[0], verts[1], verts[2] }); //Super-triangle
            }
        }
    }

    //assert(cavity.length >= 3);
    if (d.cavity.length < 4) {
        printf("Cavity does not enclose point\n");
    }
    return d.cavity.length >= 3;
}

/*
bool add_face(Delaunay& d, const Boundary& boundary) {
    const vertex_handle* verts = boundary.vertices;

    //todo sort vertices for better performance
    
    uint n = 4;
    vec3 centroid;
    for (uint i = 0; i < n; i++) {
        centroid += vertices[verts[i].id].position;
    }
    centroid /= 4;
    
    if (!find_in_circum(centroid, {})) return false;
    
    for (CavityFace& cavity : cavity) {
        tet_handle free_cell_handle = alloc_tet();
        
        tet_handle neighbor = cavity.handle;
        int index = cavity.face;
        
        vertex_handle unique;
        for (uint i = 0; i < 4 && unique.id == -1; i++) {
            unique = verts[i];
            for (uint j = 0; j < 3; j++) {
                //vertex_handle v = volume[j];
            }
        }
    }
}
*/

tet_handle alloc_tet(Delaunay& d) {
    d.tet_alloc_count++;
    tet_handle tet; // = d.free_chain;
    if (d.free.length == 0) {
        uint prev_count = d.tet_count;
        uint tet_count = max(prev_count*2, 4048);
        
        u64 elem = (sizeof(Tet) + sizeof(vertex_handle) + sizeof(tet_handle) + sizeof(float))*4;
        u64 size = elem * tet_count;
        
        //d.tets = (Tet*)realloc(d.tets, sizeof(Tet)*4*tet_count);
        d.indices = (vertex_handle*)realloc(d.indices, sizeof(vertex_handle)*4*tet_count);
        d.faces = (tet_handle*)realloc(d.faces, sizeof(tet_handle)*4*tet_count);
        d.subdets = (float*)realloc(d.subdets, sizeof(float)*4*tet_count);
        
        printf("Reallocating tet count: %i, actuall %i\n", tet_count, d.tet_alloc_count);

        /*d.tets = (Tet*)realloc(d.tets, size);
        d.indices = (vertex_handle*)((char*)d.tets + 4*sizeof(Tet)*tet_count);
        d.faces = (tet_handle*)((char*)d.indices + 4*sizeof(vertex_handle) * tet_count);
        d.subdets = (float*)((char*)d.faces + 4*sizeof(tet_handle) * tet_count);*/

        //memset(d.tets + 4*prev_count, 0, 4*sizeof(float)*(tet_count-prev_count));
        memset(d.indices + 4*prev_count, 0, 4*sizeof(vertex_handle)*(tet_count-prev_count));
        
        uint start = 4*(prev_count + (prev_count == 0)); //start at 1, 0 is reserved as null
        uint end = 4*(tet_count - 1);
        
        for (int i = end; i >= start+4; i -= 4) {
            d.subdets[i+3] = -1;
            d.free.append(i);
            //d.tets[i].next = i+4;
        }
        //d.free_chain = start + 4;
        d.tet_count = tet_count;
        tet = start;
    }
    else {
        tet = d.free.pop();
        //d.tets[tet].next;
    }
    //d.tets[tet] = {};
    d.subdets[tet+3] = 0;
    d.last = tet;

    return tet;
}

bool is_super_vert(Delaunay& d, vertex_handle v) {
    return v.id >= d.super_vert_offset && v.id < d.super_vert_offset + 4;
}

bool is_active_vert(Delaunay& d, vertex_handle v) {
    return v.id >= d.active_vert_offset && !is_super_vert(d, v);
}

bool is_or_was_active_vert(Delaunay& d, vertex_handle v) {
    return v.id >= d.prev_active_vert_offset && !is_super_vert(d, v);
}

bool add_vertex(Delaunay& d, vertex_handle vert, float min_dist = 0.0f) {
    uint res = find_in_circum(d, vert, min_dist);
    if (res == 2) return true; //too close
    if (res == 0) return false; //failed
    
    d.shared_face.capacity = d.cavity.length * 4;
    if (d.shared_face.capacity > d.max_shared_face) {
        printf("Cavity is too large!!\n");
        return false;
    }

    d.shared_face.clear();
    
    //printf("======== Retriangulating with %i faces\n", d.cavity.length);

    for (CavityFace& cavity : d.cavity) {
        tet_handle tet = alloc_tet(d);
        //printf("Alloc   %i\n", tet);

        memcpy_t(d.indices + tet, cavity.verts, 3);
        d.indices[tet+3] = vert;
        
        vec3 centroid;
        for (uint i = 0; i < 4; i++) {
            centroid += d.vertices[d.indices[tet+i].id].position;
        }
        centroid /= 4;
        //assert(point_inside(d, tet, centroid));

        //printf("====== Creating %i (%i %i %i %i) ==========\n", free_cell_handle.id, cavity.verts[0].id, cavity.verts[1].id, cavity.verts[2].id, vert.id);
        //Update doubly linked connectivity
        d.faces[tet] = cavity.tet;
        if (cavity.tet) d.faces[cavity.tet] = tet;
        
        /*
        bool eligible = true;
        for (uint i = 0; i < 4; i++) {
            vertex_handle v = d.indices[tet+i];
            if (!is_or_was_active_vert(d, v)) {
                eligible = false;
                break;
            }
        }
        
        if (eligible) {
            for (uint i = 0; i < 4; i++) {
                tet_handle face = tet+i;
                
                bool active_face = true;
                for (uint j = 0; j < 3; j++) {
                    vertex_handle v = d.indices[tet+tetra_shape.faces[i].verts[j]];
                    if (!is_active_vert(d, v)) active_face = false;
                }
                
                if (active_face) {
                    d.tets[face].prev = 0;
                    d.tets[face].next = d.active_faces;
                    assert(d.active_faces != face);
                    d.tets[d.active_faces].prev = face;
                    assert(d.tets[d.active_faces].prev != d.tets[d.active_faces].next);
                    d.active_faces = face;
                    
                    //printf("Adding link %i: %i\n", face, d.tets[face].next);
                }
            }
        }*/
        
        for (uint i = 1; i < 4; i++) {
            vertex_handle verts[3];
            for (uint j = 0; j < 3; j++) {
                verts[j] = d.indices[tet+tetra_shape.faces[i].verts[j]];
            }
            
            tet_handle face = tet+i;
            uint index = d.shared_face.add(verts);
            tet_handle& neigh = d.shared_face.values[index];
            if (!neigh) {
                neigh = face;
                d.faces[face] = {};
                //printf("Setting verts[%i], face %i: %i %i %i\n", index, i, verts[0].id, verts[1].id, verts[2].id);
            }
            else {
                //printf("Connecting with neighbor %i [%i]\n", info.neighbor.id, index);
                //assert_neighbors(volume, info.neighbor, verts);
                                    
                d.faces[neigh] = face;
                d.faces[face] = neigh;
            }
        }

        compute_subdet(d, tet);
    }

    //assert(free_cells.length == 0);

    return true;
}

void start_with_non_coplanar(CFDVolume& mesh, slice<vertex_handle> vertices) {
    uint i=0,j=1,k=2,l=3;
    uint n = vertices.length;
    bool coplanar = true;
    real orientation;
    for (i=0; coplanar && i<n-3; i++) {
        for (j=i+1; coplanar && j<n-2; j++) {
            for (k=j+1; coplanar && k<n-1; k++) {
              for (l=k+1; coplanar && l<n; l++) {
                  orientation = orient3d(mesh.vertices[i].position,
                             mesh.vertices[j].position,
                             mesh.vertices[k].position,
                             mesh.vertices[l].position);
                  
                  coplanar = fabs(orientation) < o3dstaticfilter;
              }
            }
        }
    }
    l--; k--; j--; i--;

    
    if(orientation==0.0){
        NE_WARNING("all vertices are coplanar");
    }

    // swap 0<->i  2<->j 2<->k 3<->l
    
    
    std::swap(mesh.vertices[0], mesh.vertices[i]);
    std::swap(mesh.vertices[1], mesh.vertices[j]);
    std::swap(mesh.vertices[2], mesh.vertices[k]);
    std::swap(mesh.vertices[3], mesh.vertices[l]);
}

void brio_vertices(CFDVolume& mesh, const AABB& aabb, slice<vertex_handle> vertices);

Delaunay* make_Delaunay(CFDVolume& volume, const AABB& aabb) {

//Delaunay::Delaunay(CFDVolume& volume, const AABB& aabb) : vertices(volume.vertices), volume(volume) {
    
    Delaunay* d = PERMANENT_ALLOC(Delaunay, {volume, volume.vertices});
    d->vertices = volume.vertices;
    d->max_shared_face = kb(32);
    d->aabb = aabb;
    
    LinearAllocator& allocator = get_temporary_allocator();
    
    d->shared_face = {
        d->max_shared_face,
        alloc_t<hash_meta>(allocator, d->max_shared_face),
        alloc_t<TriangleFaceSet>(allocator, d->max_shared_face),
        alloc_t<tet_handle>(allocator, d->max_shared_face)
    };
    
    vec3 centroid = aabb.centroid();
    
    float m = 1.0;
    printf("MULTIPLIER %f\n", m);
    float r = m*length(aabb.max - centroid);
    
    
    float l = sqrtf(24)*r; //Super equilateral tetrahedron side length
    float b = -r;
    float t = sqrtf(6)/3*l - r;
    float e = sqrtf(3)/3*l;
    float a = M_PI * 2 / 3;

    auto& vertices = volume.vertices;
    d->super_vert_offset = vertices.length;

    vertices.append({ centroid + vec3(e * cosf(2 * a), b, e * sinf(2 * a)) });
    vertices.append({ centroid + vec3(e * cosf(1 * a), b, e * sinf(1 * a)) });
    vertices.append({ centroid + vec3(e * cosf(0 * a), b, e * sinf(0 * a)) });

    vertices.append({ centroid + vec3(0, t, 0) });

    tet_handle super_tet = alloc_tet(*d);
    d->last = super_tet;
    
    AABB super_aabb;
    for (uint i = 0; i < 4; i++) {
        d->indices[super_tet+i] = {(int)(d->super_vert_offset+i)};
        d->faces[super_tet+i] = {};
        super_aabb.update(vertices[d->super_vert_offset + i].position);
    }
    
    exactinit(super_aabb.max.x - super_aabb.min.x, super_aabb.max.y - super_aabb.min.y, super_aabb.max.z - super_aabb.min.z); // for the predicates to work

    return d;
}

void remove_super(Delaunay& d) {
    for (tet_handle i = 4; i < 4*d.tet_count; i += 4) {
        if (d.subdets[i+3] == -1) continue;
        bool super = false;
        for (uint j = 0; j < 4; j++) {
            if (is_super_vert(d, d.indices[i+j])) {
                super = true;
                break;
            }
        }
        
        if (!super) continue;
        for (uint j = 0; j < 4; j++) {
            d.faces[d.faces[i+j]] = 0; //unlink
        }
        dealloc_tet(d, i);
    }
}

bool complete(Delaunay& d) {
    uint cell_offset = d.volume.cells.length + 1;
    
    //super_cell_base
    for (tet_handle i = 4; i < 4*d.tet_count; i += 4) {
        if (d.subdets[i+3] == -1) continue; //Assume continous
        CFDCell cell{CFDCell::TETRAHEDRON};
        for (uint j = 0; j < 4; j++) {
            cell.vertices[j] = d.indices[i+j];
            cell.faces[j].neighbor = {(int)((d.faces[i+j]&NEIGH)/4 - cell_offset)};
        }
        
        compute_normals(d.vertices, cell);
        d.volume.cells.append(cell);
    }
    
    return true;
}

bool add_vertices(Delaunay& d, slice<vertex_handle> verts, float min_dist) {
    AABB aabb;
    for (uint i = 0; i < verts.length; i++) {
        aabb.update(d.vertices[verts[i].id].position);
    }
    
    start_with_non_coplanar(d.volume, verts);
    brio_vertices(d.volume, aabb, verts);
    
    for (uint i = 0; i < verts.length; i++) {
        if (!add_vertex(d, verts[i], min_dist)) {
            draw_debug_boxes(d.volume);
            remove_super(d);
            complete(d);
            printf("Failed at %i out of %i\n", i+1, verts.length);
            return false;
        }
        if (false) {
            //draw_debug_boxes(d.volume);
            complete(d);
            return false;
        }
    }
    
    return true;
}

void destroy_Delaunay(Delaunay* delaunay) {
    //free(delaunay->tets);
    free(delaunay->indices);
    free(delaunay->faces);
    free(delaunay->active_verts);
    free(delaunay->subdets);
}

//ADVANCING FRONT DELAUNAY

bool extrude_verts(Delaunay& d, float height, float min_dist) {
    printf("Extruding verts\n");
    d.active_faces.length = 0;
    d.insertion_order.length = 0;
    
    uint n = d.vertices.length;
    
    for (uint i = d.active_vert_offset; i < n; i++) {
        VertexInfo& info = d.active_verts[i - d.active_vert_offset];
        if (info.count == 0) continue;
        
        vec3 p = d.vertices[i].position;
        vec3 n = info.normal / info.count;
        float len = height; // * info.curvature;
        
        vertex_handle v = {(int)d.vertices.length};
        d.volume.vertices.append({p + n*len});
        
        d.insertion_order.append(v);
    }
    
    if (!add_vertices(d, d.insertion_order, min_dist)) return false;
    
    d.prev_active_vert_offset = d.prev_active_vert_offset;
    d.active_vert_offset = n;
    
    return true;
}

void update_normals(Delaunay& d, vertex_handle v, vec3 normal) {
    //if (is_super_vert(d, v)) return;
    //if (!is_active_vert(d, v)) return;
    
    VertexInfo& info = d.active_verts[v.id - d.active_vert_offset];
    info.normal += normal;
    info.count++;
}

void compute_curvature(Delaunay& d, vertex_handle v, vertex_handle v2) {
    //if (is_super_vert(d, v)) return;
    //if (!is_active_vert(d, v)) return;
    
    VertexInfo& info = d.active_verts[v.id - d.active_vert_offset];
    
    vec3 c = d.vertices[v.id].position;
    vec3 p = d.vertices[v2.id].position;
    info.curvature += dot(c-p, info.normal);
}

bool generate_layer(Delaunay& d, float height) {
    //Clear
    
    uint n = d.vertices.length - d.active_vert_offset;
    
    for (uint i = 0; i < n; i++) {
        d.active_verts[i] = {};
    }
    
    for (tet_handle tet = 4; tet < 4*d.tet_count; tet += 4) {
        bool eligible = true;
        
        uint count = 0;
        
        for (uint i = 0; i < 4; i++) {
            vertex_handle v = d.indices[tet+i];
            if (is_active_vert(d, v)) count++;
            if (!is_or_was_active_vert(d, v)) {
                eligible = false;
                break;
            }
        }
        
        if (count != 3) eligible = false;
        if (!eligible) continue;
        
        for (uint i = 0; i < 4; i++) {
            tet_handle face = tet+i;
            
            bool active_face = true;
            for (uint j = 0; j < 3; j++) {
                vertex_handle v = d.indices[tet+tetra_shape[i][j]];
                if (!is_active_vert(d, v)) active_face = false;
            }
            
            if (active_face) d.active_faces.append(face);
        }
    }

    for (tet_handle active_face : d.active_faces) {
        //printf("Active face %i\n", active_face);
        tet_handle face = active_face&NEIGH_FACE;
        tet_handle tet = active_face&NEIGH;
        
        vertex_handle verts[3];
        vec3 positions[3];
        for (uint i = 0; i < 3; i++) {
            verts[i] = d.indices[tet + tetra_shape[face][i]];
            positions[i] = d.vertices[verts[i].id].position;
        }
        
        vec3 normal = triangle_normal(positions);
        
        for (uint i = 0; i < 3; i++) {
            update_normals(d, verts[i], normal);
        }
    }
    
    //active_face = d.active_faces;
    //while ((active_face = d.tets[active_face].next)) {

    for (tet_handle active_face : d.active_faces) {
        tet_handle face = active_face&NEIGH_FACE;
        tet_handle tet = active_face&NEIGH;
        
        for (uint i = 0; i < 3; i++) {
            uint index = tetra_shape[face][i];
            compute_curvature(d, d.indices[tet+index], d.indices[tet+(index+1)%3]);
        }
    }

    return extrude_verts(d, height, 0.5*height);
}

bool generate_contour(Delaunay& d, CFDSurface& surface, float height) {
    d.active_verts = (VertexInfo*)calloc(sizeof(VertexInfo), d.vertices.length);
    d.active_vert_offset = 0;
    
    for (CFDPolygon polygon : surface.polygons) {
        vec3 positions[4];
        for (uint i = 0; i < polygon.type; i++) {
            positions[i] = d.vertices[polygon.vertices[i].id].position;
        }
        
        vec3 normal = polygon.type == CFDPolygon::TRIANGLE ? triangle_normal(positions) : quad_normal(positions);
        
        for (uint i = 0; i < polygon.type; i++) {
            vertex_handle v = polygon.vertices[i];
            
            update_normals(d, v, normal);
        }
    }
    
    for (CFDPolygon polygon : surface.polygons) {
        for (uint i = 0; i < polygon.type; i++) {
            compute_curvature(d, polygon.vertices[i], polygon.vertices[(i + 1) % 3]);
        }
    }
    
    d.insertion_order.length = 0;
    for (int i = 0; i < d.super_vert_offset; i++) {
        d.insertion_order.append({i});
    }
    
    start_with_non_coplanar(d.volume, d.insertion_order);
    if (!add_vertices(d, d.insertion_order, height)) return false;
    
    //return true;
    //complete(d);
    //return false;

    return extrude_verts(d, height, 1.0*height);
}

bool add_point(Delaunay& d, vec3 pos) {
    d.vertices.append({pos});
    return add_vertex(d, {(int)(d.vertices.length-1)});
}

struct RefineTet {
    tet_handle tet;
    float skew;
};

bool refine(Delaunay& d) {
    d.active_vert_offset = 0;
    uint num_it = 3;
    for (uint i = 0; i < num_it; i++) {
        uint n = 4*d.tet_count;
        for (tet_handle i = 4; i < n; i += 4) {
            if (d.subdets[i+3] == -1) continue;
            vec3 positions[4];
            bool super = false;
            
            for (uint j = 0; j < 4; j++) {
                vertex_handle v = d.indices[i+j];
                //|| v.id < d.active_vert_offset
                if (is_super_vert(d, v) || v.id < d.active_vert_offset) super = true;
                //if (!d.faces[i+j]) super = true;
                positions[j] = d.vertices[v.id].position;
            }
            
            if (super) continue;
            
            float longest_edge = FLT_MAX;
            
            for (uint a = 0; a < 4; a++) {
                for (uint b = a+1; b < 4; b++) {
                    longest_edge = fminf(longest_edge, sq_distance(positions[a], positions[b]));
                }
            }
            longest_edge = sqrtf(longest_edge);
            
            Circumsphere circum = circumsphere(positions);
            
            //circum.radius / longest_edge > 0.8
            //2*circum.radius / longest_edge > 0.8
            //circum.radius / longest_edge > 2
            //tetrahedron_skewness(positions) > 0.3
            if (tetrahedron_skewness(positions) > 0.3) {
                vec3 pos = circumsphere(positions).center;
                
                if (!d.aabb.inside(pos)) pos = (positions[0]+positions[1]+positions[2]+positions[3])/4;
                
                d.last = i;
                if (!add_point(d, pos)) return false;
            }
        }
    }
    
    return true;
}

/*struct Edge {
    vertex_handle a;
    vertex_handle b;
};*/

bool smooth(Delaunay& d) {
    LinearRegion region(get_temporary_allocator());
    vec4* avg = TEMPORARY_ZEROED_ARRAY(vec4, d.vertices.length);
    
    uint num_it = 5;
    for (uint i = 0; i < num_it; i++) {
        uint n = 4*d.tet_count;
        for (tet_handle i = 4; i < n; i += 4) {
            for (uint a = 0; a < 4; a++) {
                for (uint b = a+1; b < 4; b++) {
                    uint v = d.indices[i+a].id;
                    uint v2 = d.indices[i+b].id;
                    
                    vec3 pos1 = d.vertices[v].position;
                    vec3 pos2 = d.vertices[v2].position;
                    
                    float weight = 1.0;
                    
                    // && v < d.active_vert_offset
                    // && v2 < d.active_vert_offset
                    if ((v < d.grid_vert_begin  || v >= d.grid_vert_end) && v < d.active_vert_offset) avg[v] += vec4(pos2, weight);
                    if ((v2 < d.grid_vert_begin || v2 >= d.grid_vert_end) && v2 < d.active_vert_offset) avg[v2] += vec4(pos1, weight);
                    
                    //vec3 position = d.vertices[d.indices[i+b].id].position;
                    //avg[v] += vec4(position, weight);
                }
            }
        }
        
        for (uint i = 0; i < d.vertices.length; i++) {
            vec3 orig = d.vertices[i].position;
            vec3 smoothed = avg[i].w == 0 ? orig : vec3(avg[i]) / avg[i].w;
            d.vertices[i].position = smoothed;
            //vec3(avg[i]) / avg[i].w;
            avg[i] = {};
        }
    }
}

void enqueue_vertex(Delaunay& d, vec3 pos) {
    d.vertices.append({pos});
    d.insertion_order.append({(int)d.vertices.length-1});
}

void generate_grid(Delaunay& d) {
    AABB& aabb = d.aabb;
    vec3 size = aabb.size();
    float r = 20.0f;
    
    d.insertion_order.length = 0;
    d.grid_vert_begin = d.vertices.length;
    for (float n = 0; n < 2.0; n++) {
        for (real a = 0; a < size.x+r; a += r) { //top & bottom
            for (real b = 0; b < size.z+r; b += r) {
                enqueue_vertex(d, aabb.min + vec3(a,n*size.y,b));
            }
        }
        for (real a = r; a < size.x; a += r) { //front & back
            for (real b = r; b < size.y; b += r) {
                enqueue_vertex(d, aabb.min + vec3(a,b,n*size.z));
            }
        }
        for (real a = r; a < size.y; a += r) { //left & right
            for (real b = 0; b < size.z+r; b += r) {
                enqueue_vertex(d, aabb.min + vec3(n*size.x,a,b));
            }
        }
    }
    d.grid_vert_end = d.vertices.length;
    
    if (!add_vertices(d, d.insertion_order)) return;
}

void generate_n_layers(Delaunay& d, CFDSurface& surface, uint n, float initial, float g) {
    if (!generate_contour(d, surface, initial)) return;
    printf("Generated contour\n");
    
    float h = initial;
    for (uint i = 1; i < n; i++) {
        h *= g;
        if (!generate_layer(d, h)) return;
        printf("Generated layer %i out of %i\n", i+1, n);
    }
    
    generate_grid(d);
    printf("Generated grid!\n");
    remove_super(d);
    d.active_vert_offset = d.vertices.length;
    
    if (!refine(d)) {
        complete(d);
        return;
    }
    smooth(d);
    complete(d);
}


#if 0
    uint n = 1;
    for (uint i = 0; i < n; i++) {
        uint n = volume.cells.length;
        for (int i = super_cell_offset; i < n; i++) {
            CFDCell& cell = volume.cells[i];
            vec3 positions[4];

            get_positions(volume.vertices, { cell.vertices,4 }, positions);
            float skewness = tetrahedron_skewness(positions);

            static char empty[sizeof(vec4)];
            
            if (memcmp(&bowyer.subdets[i], empty, sizeof(vec4)) == 0) continue;
            
            if (skewness > 0.9) {
                //Circumsphere circum = circumsphere(positions);
                //if (circum.radius < 0) continue;
                vec3 circum_center = centroid4(volume, cell.vertices); // circum.center;
                
                vertex_handle v = { (int)volume.vertices.length };
                volume.vertices.append({ circum_center });
                bowyer.last = {i};
                bowyer.add_vertex(v);
            }
        }
    }
#endif

#if 0
void insphere_test() {
    CFDVolume mesh;
    
    mesh.vertices.append({vec3(-1,0,0)});
    mesh.vertices.append({vec3(1,0,0)});
    mesh.vertices.append({vec3(0,0,1)});
    mesh.vertices.append({vec3(0,1.0,0.5)});
    
    CFDCell cell{CFDCell::TETRAHEDRON};
    for (int i = 0; i < 4; i++) {
        cell.vertices[i] = {i};
    }
    mesh.cells.append(cell);

    vec4 subdets[4];
    compute_subdet(mesh, {subdets,4}, {0});

    mesh.vertices.append({vec3(0,0.5,0.5)});
    assert(insphere(mesh, {subdets,4}, {0}, {4}) > 0);
    
    mesh.vertices.append({vec3(0,0.0,0.0)});
    assert(insphere(mesh, {subdets,4}, {0}, {5}) > 0);
    
    mesh.vertices.append({vec3(-1.1,0,0)});
    assert(insphere(mesh, {subdets,4}, {0}, {6}) < 0);
}
#endif
