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

const tet_handle NEIGH = (1 << 30)-1 << 2;
const tet_handle NEIGH_FACE = (1<<2)-1;

struct Delaunay {
    //Delaunay
    CFDVolume& volume;
    slice<CFDVertex> vertices;
    hash_map_base<TriangleFaceSet, tet_handle> shared_face;
    AABB aabb;
    uint max_shared_face;
    uint super_offset;
    tvector<CavityFace> cavity;
    tet_handle last;
    tet_handle free_chain;

    //Mesh
    uint tet_count;
    Tet* tets;
    vertex_handle* indices;
    tet_handle* faces;
    float* subdets;

    //Advancing front
    tet_handle active_faces;
    VertexInfo* active_verts;
    uint active_vert_offset;
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
    p[4] = vertices[v.id].position;
    
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
    auto indices = d.indices;
    auto vertices = d.vertices;
    
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

bool point_inside(CFDVolume& volume, cell_handle cell_handle, vec3 pos) {
    CFDCell& cell = volume.cells[cell_handle.id];

    for (uint i = 0; i < 4; i++) {
        vec3 p = volume.vertices[cell.vertices[tetra_shape.faces[i].verts[0]].id].position;
        float d = dot(cell.faces[i].normal, pos - p);
        if (d > FLT_EPSILON) return false;
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
        add_box_cell(volume, p, 0.01);
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

tet_handle find_enclosing_tet(Delaunay& d, vec3 pos) {
    //Walk structure to get to enclosing triangle, starting from the last inserted
    tet_handle current = d.last;
    tet_handle prev = -1;

    //printf("==============\n");
    //printf("Starting at %i\n", current.id);

    uint count = 0;
    loop: {
        uint offset = rand();
        for (uint it = 0; it < 4; it++) {
            uint i = (it + offset) % 4;
            tet_handle neighbor = d.faces[current + i] & NEIGH;
            if (!neighbor || neighbor == prev) continue;

            vec3 verts[3];
            for (uint j = 0; j < 3; j++) {
                verts[j] = d.vertices[d.indices[current + j].id].position;
            }

            float det = -orient3d(verts[0], verts[1], verts[2], pos);
            if (det > FLT_EPSILON) {
                prev = current;
                current = neighbor;
                if (count++ > 1000) return 0;
                goto loop;
            }
        }
    };

    return current;
}

bool find_in_circum(Delaunay& d, vertex_handle v) {
    vec3 pos = d.vertices[v.id].position;
    tet_handle current = find_enclosing_tet(d, pos);
    if (!current) return false;

    d.cavity.length = 0;

    d.subdets[current + 3] = -1;

    d.tets[current].next = d.free_chain; //store free chain in next
    d.tets[current].prev = 0; //store stack in prev

    tet_handle stack = current;
    tet_handle free = current;

    while (stack != -1) {
        current = stack;
        Tet& cell = d.tets[current];
        stack = cell.prev;

        //printf("Visiting %i\n", current.id);
        for (uint i = 0; i < 4; i++) {
            tet_handle neighbor = d.faces[current+i]&NEIGH;

            vertex_handle verts[4];
            for (uint j = 0; j < 3; j++) {
                verts[j] = d.indices[current + tetra_shape.faces[i].verts[2 - j]];
                //Reverse orientation, as tetrashape will reverse orientation when extruding
                //Reverse-reverse -> no face flip -> as face should point outwards
            }


            if (neighbor != -1) {
                static char empty[sizeof(vec4)];
                
                bool visited = d.subdets[neighbor+3] == -1;
                bool inside = visited;
                if (!visited) {
                    float value = insphere(d, neighbor, v);
                    //printf("Insphere %f\n", value);
                    inside |= value < 0;
                }
                
                if (!visited && inside) {
                    //printf("Neighbor %i: adding to stack\n", neighbor.id);

                    d.subdets[neighbor+3] = -1; //Mark for deletion and that it is already visited
                    
                    d.tets[neighbor].next = d.free_chain;
                    d.tets[neighbor].prev = stack;
                    stack = neighbor;
                    free = neighbor;
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
                d.cavity.append({ current + i, verts[0], verts[1], verts[2] }); //Super-triangle
            }
        }
    }
    
    d.free_chain = free;

    //assert(cavity.length >= 3);
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
    tet_handle tet = d.free_chain;
    if (!tet) {
        uint prev_count = d.tet_count;
        uint tet_count = prev_count*2;
        
        u64 size = (sizeof(Tet) + sizeof(vertex_handle) + sizeof(tet_handle) + sizeof(float))* 4*d.tet_count;

        d.tets = (Tet*)realloc(d.tets, size);
        d.indices = (vertex_handle*)((char*)d.tets + 4*sizeof(Tet)*tet_count);
        d.faces = (tet_handle*)((char*)d.indices + 4*sizeof(vertex_handle) * tet_count);
        d.subdets = (float*)((char*)d.faces + 4*sizeof(TetFaces) * tet_count);

        d.free_chain = prev_count;
        for (uint i = prev_count; i < tet_count-1; i++) {
            d.tets[i].next = i+1;
            d.tets[i].prev = 0;
        }
        d.tets[tet_count - 1] = {};
        d.tet_count = tet_count;
    }
    else {
        d.free_chain = d.tets[tet].next;
    }
    d.tets[tet].next = {};
    d.subdets[tet+3] = -1;

    return tet;
}

bool add_vertex(Delaunay& d, vertex_handle vert) {
    if (!find_in_circum(d, vert)) return false;
    
    d.shared_face.capacity = d.cavity.length * 3;
    assert(d.shared_face.capacity < d.max_shared_face);
    d.shared_face.clear();
    
    //printf("======== Retriangulating with %i faces\n", cavity.length);

    for (CavityFace& cavity : d.cavity) {
        tet_handle tet = alloc_tet(d);

        memcpy_t(d.indices + tet, cavity.verts, 3);
        d.indices[tet+3] = vert;

        //printf("====== Creating %i (%i %i %i %i) ==========\n", free_cell_handle.id, cavity.verts[0].id, cavity.verts[1].id, cavity.verts[2].id, vert.id);
        //Update doubly linked connectivity
        d.faces[tet] = cavity.tet;
        if (!cavity.tet) d.faces[cavity.tet] = tet;
        
        for (uint i = 1; i < 4; i++) {
            vertex_handle verts[3];
            for (uint j = 0; j < 3; j++) {
                verts[j] = d.indices[tet+tetra_shape.faces[i].verts[j]];
            }
            
            uint index = d.shared_face.add(verts);
            tet_handle& neigh = d.shared_face.values[index];
            if (!neigh) {
                neigh = tet+i;
                d.faces[tet+i] = {};
                //printf("Setting verts[%i], face %i: %i %i %i\n", index, i, verts[0].id, verts[1].id, verts[2].id);
            }
            else {
                //printf("Connecting with neighbor %i [%i]\n", info.neighbor.id, index);
                //assert_neighbors(volume, info.neighbor, verts);
                                    
                d.faces[neigh] = tet+i;
                d.faces[tet + i] = neigh;
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

Delaunay::Delaunay(CFDVolume& volume, const AABB& aabb) : vertices(volume.vertices), volume(volume) {
    max_shared_face = 10000;
    
    LinearAllocator& allocator = get_temporary_allocator();
    
    shared_face = {
        max_shared_face,
        alloc_t<hash_meta>(allocator, max_shared_face),
        alloc_t<TriangleFaceSet>(allocator, max_shared_face),
        alloc_t<FaceInfo>(allocator, max_shared_face)
    };
    
    float r = max(aabb.size()) / 2.0f;
    vec3 centroid = aabb.centroid();
    
    float l = sqrtf(24)*r; //Super equilateral tetrahedron side length
    float b = -r;
    float t = sqrtf(6)/3*l - r;
    float e = sqrtf(3)/3*l;
    float a = M_PI * 2 / 3;

    auto& vertices = volume.vertices;
    super_vertex_base = {(int)vertices.length};

    vertices.append({ centroid + vec3(e * cosf(2 * a), b, e * sinf(2 * a)) });
    vertices.append({ centroid + vec3(e * cosf(1 * a), b, e * sinf(1 * a)) });
    vertices.append({ centroid + vec3(e * cosf(0 * a), b, e * sinf(0 * a)) });

    vertices.append({ centroid + vec3(0, t, 0) });

    CFDCell cell{CFDCell::TETRAHEDRON};
    vec3 positions[4];
    
    exactinit(aabb.max.x - aabb.min.x, aabb.max.y - aabb.min.y, aabb.max.z - aabb.min.z); // for the predicates to work
}

bool Delaunay::add_vertices(slice<vertex_handle> verts) {
    brio_vertices(volume, aabb, verts);
    start_with_non_coplanar(volume, verts);
    
    for (uint i = 0; i < verts.length; i++) {
        if (!add_vertex(verts[i])) return false;
    }
    
    return true;
}

bool Delaunay::complete() {
    //super_cell_base
    for (uint i = 0; i < volume.cells.length; i++) {
        CFDCell& cell = volume.cells[i];
        bool shared = false;
        for (uint j = 0; j < 4; j++) {
            uint index = cell.vertices[j].id;
            if (index >= super_vertex_base.id && index < super_vertex_base.id + 4) {
                shared = true;
                break;
            }
        }

        if (shared) {
            for (uint j = 0; j < 4; j++) cell.vertices[j].id = 0;
        }
    }
    
    return true;
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