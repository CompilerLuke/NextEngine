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


static real insphere(CFDVolume& mesh, slice<vec4> subdets, cell_handle curr, vertex_handle v) {
    CFDCell& cell = mesh.cells[curr.id];
    vec4 subdet = subdets[curr.id];
    
#if 1
    vec3 p[5];
    for (uint i = 0; i < 4; i++) p[i] = mesh[cell.vertices[i]].position;
    p[4] = mesh[v].position;
    
    real det = insphere(&p[0].x, &p[1].x, &p[2].x, &p[3].x, &p[4].x);
    
    if (det == 0.0) {
        vertex_handle nn[5];
        memcpy_t(nn, cell.vertices, 4);
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
void compute_subdet(CFDVolume& mesh, slice<vec4> subdets, cell_handle cell_handle)
{
    vec4 ab;
    vec4 ac;

    CFDCell& cell = mesh[cell_handle];
    vec4& subdet = subdets[cell_handle.id];

    vec3 a = mesh[cell.vertices[0]].position;
    vec3 b = mesh[cell.vertices[1]].position;
    vec3 c = mesh[cell.vertices[2]].position;

    if(cell.vertices[3].id != -1) {
        vec3 d = mesh[cell.vertices[3]].position;
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
        subdet.x = ab.w*cd12 + ac.w*db12 + ad.w*bc12;
        subdet.y = ab.z*cd30 + ac.z*db30 + ad.z*bc30;
        subdet.z = ab.y*cd30 + ac.y*db30 + ad.y*bc30;
        subdet.w = ab.x*cd12 + ac.x*db12 + ad.x*bc12;

    // SubDet[3]=AD*(ACxAB) = 6 X volume of the tet !
    }
    else {
        uint i;
        for (i=0; i<3; i++) {
          ab[i]=b[i]-a[i]; //AB
          ac[i]=c[i]-a[i]; //AC
        }

        vec3 c = cross(ac, ab);
        subdet = vec4(c,dot(c,c));
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



bool Delaunay::find_in_circum(vec3 pos, vertex_handle v) {
    //Walk structure to get to enclosing triangle, starting from the last inserted
    cell_handle current = last;
    cell_handle prev;

    //printf("==============\n");
    //printf("Starting at %i\n", current.id);

    uint count = 0;
    loop: {
        CFDCell& cell = volume[current];

        uint offset = rand();
        for (uint it = 0; it < 4; it++) {
            uint i = (it + offset) % 4;
            cell_handle neighbor = cell.faces[i].neighbor;
            if (neighbor.id == -1 || neighbor.id == prev.id) continue;

            vec3 verts[3];
            get_positions(volume.vertices, cell, tetra_shape.faces[i], verts);
            
            vec3 vert0 = volume[cell.vertices[tetra_shape[i][0]]].position;
            vec3 normal = cell.faces[i].normal;
            
            float det = dot(normal, pos - vert0);
            float det2 = -orient3d(verts[0], verts[1], verts[2], pos);
            
            if (det2 > FLT_EPSILON) {
                prev = current;
                current = neighbor;
                if (count++ > 1000) {
                    return false;
                }
                goto loop;
            }
        }
    };

    CFDCell& start = volume[current];

    ///assert(inside == 1);

    /*bool inside = point_inside(volume, current, pos);
    assert(inside);
    
    bool inside_circum = point_inside(circumspheres[current.id], pos);

    if (!inside_circum) {
        printf("Failed current at %i\n", current.id);
        //add_box_cell(volume, orig, 0.2);
        //for (uint i = 0; i < 4; i++) {
        //    add_box_cell(volume, volume.vertices[start.vertices[i].id].position, 0.1);
        //}
        //add_box_cell(volume, centroid4(volume, start.vertices), 0.1);
        


        return false;
    }*/
    
    stack.length = 0;
    cavity.length = 0;

    memset(&subdets[current.id], 0, sizeof(vec4));
    stack.append(current);
    free_cells.append(current);
    while (stack.length > 0) {
        current = stack.pop();
        CFDCell& cell = volume.cells[current.id];

        //printf("Visiting %i\n", current.id);
        for (uint i = 0; i < 4; i++) {
            vertex_handle verts[3];
            for (uint j = 0; j < 3; j++) {
                verts[j] = cell.vertices[tetra_shape.faces[i].verts[2-j]];
                //Reverse orientation, as tetrashape will reverse orientation when extruding
                //Reverse-reverse -> no face flip -> as face should point outwards
            }


            cell_handle neighbor = cell.faces[i].neighbor;

            if (neighbor.id != -1) {
                static char empty[sizeof(vec4)];
                
                bool visited = memcmp(&subdets[neighbor.id], empty, sizeof(vec4)) == 0;
                bool inside = visited;
                if (!visited) {
                    float value = insphere(volume, subdets, neighbor, v);
                    //printf("Insphere %f\n", value);
                    inside |= value < 0;
                }
                
                
                if (!visited && inside) {
                    //printf("Neighbor %i: adding to stack\n", neighbor.id);

                    memset(&subdets[neighbor.id], 0, sizeof(vec4)); //Mark for deletion and that it is already visited
                    assert(memcmp(&subdets[neighbor.id], empty, sizeof(vec4)) == 0);
                    stack.append(neighbor);
                    free_cells.append(neighbor);
                }

                //printf("Face %i, neighbor %i, %s\n", i, neighbor.id, inside ? "not unique" : "unique");
                if (!inside) { //face is unique
                    //printf("Neighbor %i: Unique face %i\n", neighbor.id, i);
                    assert_neighbors(volume, neighbor, verts);

                    cavity.append({ neighbor, {verts[0], verts[1], verts[2]} });
                }

                //if (visited) printf("Neighbor %i: already visited %i\n", neighbor.id, i);
            }
            else {
                //printf("Outside\n");
                cavity.append({ {}, {verts[0], verts[1], verts[2]} }); //Super-triangle
            }
        }
    }
    

    //assert(cavity.length >= 3);
    return cavity.length >= 3;
}


bool Delaunay::update_circum(cell_handle cell_handle) {
    CFDCell& cell = volume.cells[cell_handle.id];

    vec3 positions[4];
    get_positions(volume.vertices, { cell.vertices, 4 }, positions);

    vec3 centroid;
    for (uint i = 0; i < 4; i++) centroid += positions[i] / 4.0f;

    //printf("Created %i, center: %f %f %f, radius: %f\n", cell_handle.id, circum.center.x, circum.center.y, circum.center.z, circum.radius);
    //for (uint i = 0; i < 4; i++) {
    //    printf("    V[%i] = (%f,%f,%f)\n", i, positions[i].x, positions[i].y, positions[i].z);
    //}

    if (cell_handle.id >= subdets.length) {
        uint capacity = max(cell_handle.id+1, subdets.capacity * 2);
        subdets.reserve(capacity);
        subdets.length = capacity;
    }
    
    compute_subdet(volume, subdets, cell_handle);
    compute_normals(volume.vertices, cell);

    return true;
}

//would not be necessary, if this data was stored in cell connectivity
inline int Delaunay::get_face_index(const TriangleFaceSet& face, CFDCell& neighbor) {
    for (uint i = 0; i < 4; i++) {
        vertex_handle verts[3];
        for (uint j = 0; j < 3; j++) {
            verts[j] = neighbor.vertices[tetra_shape[i][j]];
        }

        if (face == verts) return i;
    }
    
    return -1;
}

bool Delaunay::add_face(const Boundary& boundary) {
    const vertex_handle* verts = boundary.vertices;

    //todo sort vertices for better performance
    
    uint n = 4;
    vec3 centroid;
    for (uint i = 0; i < n; i++) {
        centroid += volume[verts[i]].position;
    }
    centroid /= 4;
    
    if (!find_in_circum(centroid, {})) return false;
    
    for (CavityFace& cavity : cavity) {
        cell_handle free_cell_handle;
        if (free_cells.length > 0) {
            free_cell_handle = free_cells.pop();
        } else {
            free_cell_handle = { (int)volume.cells.length };
            volume.cells.append({ CFDCell::PENTAHEDRON });
        }
        
        cell_handle neighbor = cavity.handle;
        int index = get_face_index(verts, volume[neighbor]);
        
        vertex_handle unique;
        for (uint i = 0; i < 4 && unique.id == -1; i++) {
            unique = verts[i];
            for (uint j = 0; j < 3; j++) {
                //vertex_handle v = volume[j];
            }
        }
    }
}

bool Delaunay::add_vertex(vertex_handle vert) {

    vec3 position = volume.vertices[vert.id].position;

    if (!find_in_circum(position, vert)) return false;
    
    shared_face.capacity = cavity.length * 3;
    assert(shared_face.capacity < max_shared_face);
    shared_face.clear();
    
    //printf("======== Retriangulating with %i faces\n", cavity.length);

    for (CavityFace& cavity : cavity) {
        cell_handle free_cell_handle;
        if (free_cells.length > 0) {
            free_cell_handle = free_cells.pop();
        }
        else {
            free_cell_handle = { (int)volume.cells.length };
            volume.cells.append({ CFDCell::TETRAHEDRON });
        }
        CFDCell& free_cell = volume.cells[free_cell_handle.id];

        memcpy_t(free_cell.vertices, cavity.verts, 3);
        free_cell.vertices[3] = vert;

        vec3 points[4];
        for (uint i = 0; i < 4; i++) {
            points[i] = volume.vertices[free_cell.vertices[i].id].position;
        }

        //Update doubly linked connectivity
        free_cell.faces[0].neighbor = cavity.handle;
        
        //printf("====== Creating %i (%i %i %i %i) ==========\n", free_cell_handle.id, cavity.verts[0].id, cavity.verts[1].id, cavity.verts[2].id, vert.id);
        if (cavity.handle.id != -1) {
            CFDCell& neighbor = volume.cells[cavity.handle.id];
            int index = get_face_index(cavity.verts, neighbor);
            neighbor.faces[index].neighbor = free_cell_handle;
        }
        else {
            //printf("Outside\n");
        }
        
        for (uint i = 1; i < 4; i++) {
            vertex_handle verts[3];
            for (uint j = 0; j < 3; j++) {
                verts[j] = free_cell.vertices[tetra_shape.faces[i].verts[j]];
            }
            
            uint index = shared_face.add(verts);
            FaceInfo& info = shared_face.values[index];
            if (info.face == -1) {
                info.neighbor = free_cell_handle;
                info.face = i;
                free_cell.faces[i].neighbor = {};

                //printf("Setting verts[%i], face %i: %i %i %i\n", index, i, verts[0].id, verts[1].id, verts[2].id);
            }
            else {
                //printf("Connecting with neighbor %i [%i]\n", info.neighbor.id, index);
                assert_neighbors(volume, info.neighbor, verts);
                                    
                cell_handle neighbor = info.neighbor;
                volume.cells[neighbor.id].faces[info.face].neighbor = free_cell_handle;
                free_cell.faces[i].neighbor = neighbor;
            }
        }

        if (!update_circum(free_cell_handle)) return false;
        last = free_cell_handle;
    }

    //assert(free_cells.length == 0);

    return true;
}


#include <algorithm>


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

Delaunay::Delaunay(CFDVolume& volume, const AABB& aabb) : volume(volume) {
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

    super_vertex_base = {(int)volume.vertices.length};

    volume.vertices.append({ centroid + vec3(e * cosf(2 * a), b, e * sinf(2 * a)) });
    volume.vertices.append({ centroid + vec3(e * cosf(1 * a), b, e * sinf(1 * a)) });
    volume.vertices.append({ centroid + vec3(e * cosf(0 * a), b, e * sinf(0 * a)) });

    volume.vertices.append({ centroid + vec3(0, t, 0) });

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
