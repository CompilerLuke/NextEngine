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

//#define MATRIX_CIRCUM_IMPL

Circumsphere circumsphere(vec3 p[4]) {
#ifndef MATRIX_CIRCUM_IMPL
	vec3 u1 = p[0] - p[3];
	vec3 u2 = p[1] - p[3];
	vec3 u3 = p[2] - p[3];

	vec3 center = (sq_distance(p[0], p[3]) * cross(u2, u3)
		+ sq_distance(p[1], p[3]) * cross(u3, u1)
		+ sq_distance(p[2], p[3]) * cross(u1, u2))
		/ (2 * dot(u1, cross(u2, u3)));
	center += p[3];
	float r = length(center - p[0]);
    if (r < 0) { //co-planar
        center = vec3(0);
        r = 100000;
    }
    return {center, r};
	
#else

	glm::mat3 a = glm::transpose(glm::mat3(p[0] - p[3], p[1] - p[3], p[2] - p[3]));
	float sp = sq(p[3]);
	vec3 b = 0.5f * vec3(sq(p[0]) - sp, sq(p[1]) - sp, sq(p[2]) - sp);
	vec3 c = glm::inverse(a) * b;

	float det = (
		+a[0][0] * (a[1][1] * a[2][2] - a[2][1] * a[1][2])
		- a[1][0] * (a[0][1] * a[2][2] - a[2][1] * a[0][2])
		+ a[2][0] * (a[0][1] * a[1][2] - a[1][1] * a[0][2]));


    float r = length(c - p[0]);
	if (fabs(det) < 0.01) {
		//printf("Co-planar points\n");
		c = (p[0] + p[1] + p[2] + p[3]) / 4;
        for (uint i = 1; i < 4; i++) {
            r = fmaxf(r, length(p[i] - c));
        }
	//return vec4((p[0]+p[1]+p[2]+p[3])/3, length(p[0]-p[1]));
	}

	//assert(det != 0.0f);
	//assert(r < 1000);
	//assert(!isnan(c.x) && !isnan(c.y) && !isnan(c.z));

    return {c,r};
#endif
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

bool ray_triangle_intersection(vec3 orig, vec3 dir, vec3 p[3], float* t) {
    vec3 v0v1 = p[1] - p[0];
    vec3 v0v2 = p[2] - p[0];

    vec3 pvec = cross(dir, v0v2);

    float det = dot(v0v1, pvec);

    if (fabs(det) < FLT_EPSILON) {
        //printf("Parallel\n");
        return false;
    }

    float invDet = 1.0 / det;

    vec3 tvec = orig - p[0];

    float u = dot(tvec, pvec) * invDet;

    if (u < 0 || u > 1)
        return false;

    vec3 qvec = cross(tvec, v0v1);

    float v = dot(dir, qvec) * invDet;

    if (v < 0 || u + v > 1)
        return false;

    *t = dot(v0v2, qvec) * invDet;
    return *t > 0;
    
    /*
    vec3 v0v1 = v[1] - v[0];
    vec3 v0v2 = v[2] - v[0];

    vec3 n = cross(v0v1, v0v2);

    float n_dot_ray = dot(n, dir);
    if (fabs(n_dot_ray) < FLT_EPSILON) return false;

    //*t = dot(n, v[0] - orig) / n_dot_ray;
    *t = (dot(n, orig) + dot(n, v[0])) / n_dot_ray;

    if (*t < 0) return false;

    vec3 p = orig + *t * dir;
    vec3 c;

    vec3 edge0 = v[1] - v[0];
    vec3 vp0 = p - v[0];
    c = cross(edge0, vp0);
    if (dot(n, c) < 0) return false;

    vec3 edge1 = v[2] - v[1];
    vec3 vp1 = p - v[1];
    c = cross(edge1, vp1);
    if (dot(n, c) < 0) return false;

    vec3 edge2 = v[0] - v[2];
    vec3 vp2 = p - v[2];
    c = cross(edge2, vp2);
    if (dot(n, c) < 0) return false;

    return true;*/
}

struct FaceInfo {
    int face = -1;
    cell_handle neighbor;
};

struct CavityFace {
    cell_handle handle;
    vertex_handle verts[3];
};

/*
float min_dist = length(circumspheres[current.id].center - pos);

        while (true) {
            auto& faces = volume.cells[current.id].faces;

            cell_handle closest_neighbor = {};
            if (!point_inside(circumspheres[current.id], pos)) {
                min_dist = FLT_MAX;
            }
            else {
                break;
            }

            for (uint i = 0; i < 4; i++) {
                cell_handle neighbor = faces[i].neighbor;
                if (neighbor.id == -1) continue;

                vec3 positions[4];
                get_positions(volume.vertices, { volume.cells[neighbor.id].vertices, 4 }, positions);

                vec3 centroid;
                for (uint i = 0; i < 4; i++) {
                    centroid += positions[i] / 4.0f;
                }

                float dist = length(centroid - pos); //todo avoid sqrtf
                if (dist < min_dist) {
                    closest_neighbor = neighbor;
                    min_dist = dist;
                }
            }

            if (closest_neighbor.id == -1) break;
            current = closest_neighbor;
        }
*/

vec3 centroid4(CFDVolume& volume, vertex_handle vertices[4]) {
    vec3 centroid;
    for (uint i = 0; i < 4; i++) {
        centroid += volume.vertices[vertices[i].id].position;
    }
    centroid /= 4.0f;
    return centroid;
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

cell_handle neighbor_towards_ray(CFDVolume& volume, vec3 orig, vec3 dir, float max_t, cell_handle prev, cell_handle current) {
    CFDCell& cell = volume.cells[current.id];

    for (uint i = 0; i < 4; i++) {
        cell_handle neighbor = cell.faces[i].neighbor;
        //if (neighbor.id == -1) printf("Outside\n");
        //if (prev.id == neighbor.id) printf("Coming from that one!\n");
        if (neighbor.id == -1 || prev.id == neighbor.id) continue;

        vec3 centroid;
        vertex_handle verts[3];
        vec3 positions[3];
        for (uint j = 0; j < 3; j++) {
            verts[j] = cell.vertices[tetra_shape.faces[i].verts[j]];
            positions[j] = volume.vertices[verts[j].id].position;
            centroid += positions[j] / 3;
        }

        float t = 0.0f;
        bool inter = ray_triangle_intersection(orig, dir, positions, &t);

        if (inter && t < max_t) {
            vec3 p = orig + t * dir;
            add_box_cell(volume, p, 0.1);

            //printf("%i => %i, face: %i, t: %f, max: %f\n", prev.id, current.id, i, t, max_t);

            //draw_ray(volume, orig, dir, t);
            assert_neighbors(volume, current, verts);
            return neighbor;
        }
    }

    //draw_ray(volume, orig, dir, max_t);

    return {};
}

struct BowyerWatson {
    CFDVolume& volume;
    hash_map_base<TriangleFaceSet, FaceInfo> shared_face;
    uint max_shared_face;
    uint super_offset;
    tvector<CavityFace> cavity;
    tvector<Circumsphere> circumspheres;
    tvector<cell_handle> stack;
    tvector<cell_handle> free_cells;
    cell_handle last;
    

    bool find_in_circum(vec3 pos) {
        //Walk structure to get to enclosing triangle, starting from the last inserted
        cell_handle current = last;
        cell_handle prev;

        
        //printf("==============\n");
        //printf("Starting at %i\n", current.id);
        CFDCell& cell = volume.cells[current.id];
        vec3 orig = centroid4(volume, cell.vertices);
        vec3 dir = pos - orig;
        float max_t = length(dir);
        assert(max_t > 0);
        dir /= max_t;
        
        /*loop: {
            cell_handle next = neighbor_towards_ray(volume, orig, dir, max_t, prev, current);
            if (next.id != -1) {
                prev = current;
                current = next;
                goto loop;
            }
            else if (!point_inside(circumspheres[current.id], pos)) {
                auto& faces = volume.cells[current.id].faces;
                prev = current;
                for (uint i = 0; i < 4; i++) {
                    cell_handle next = neighbor_towards_ray(volume, orig, dir, max_t, prev, faces[i].neighbor);
                    if (next.id != -1) {
                        prev = faces[i].neighbor;
                        current = next;
                        goto loop;
                    }
                }

            }
        } */

        uint count = 0;
        loop: {
            CFDCell& cell = volume.cells[current.id];

            uint offset = rand();
            for (uint it = 0; it < 4; it++) {
                uint i = (it + offset) % 4;
                cell_handle neighbor = cell.faces[i].neighbor;
                if (neighbor.id == -1 || neighbor.id == prev.id) continue;

                vec3 vert0 = volume.vertices[cell.vertices[tetra_shape.faces[i].verts[0]].id].position;
                vec3 normal = cell.faces[i].normal;
                
                if (dot(normal, pos - vert0) > FLT_EPSILON) {
                    prev = current;
                    current = neighbor;
                    if (count++ > 1000) {
                        return false;
                    }
                    goto loop;
                }
            }
        };

        CFDCell& start = volume.cells[current.id];

        ///assert(inside == 1);
        //test if inside
        float dist = length(circumspheres[current.id].center - pos);
        float r = circumspheres[current.id].radius;

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

        circumspheres[current.id].radius = 0;
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
                    bool visited = circumspheres[neighbor.id].radius == 0;
                    bool inside = visited || point_inside(circumspheres[neighbor.id], pos, FLT_EPSILON);
                    
                    if (!visited && inside) {
                        //printf("Neighbor %i: adding to stack\n", neighbor.id);
                        circumspheres[neighbor.id].radius = 0; //Mark for deletion and that it is already visited
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

    bool update_circum(cell_handle cell_handle) {
        CFDCell& cell = volume.cells[cell_handle.id];

        vec3 positions[4];
        get_positions(volume.vertices, { cell.vertices, 4 }, positions);

        vec3 centroid;
        for (uint i = 0; i < 4; i++) centroid += positions[i] / 4.0f;

        Circumsphere circum = circumsphere(positions);
        //printf("Created %i, center: %f %f %f, radius: %f\n", cell_handle.id, circum.center.x, circum.center.y, circum.center.z, circum.radius);
        //for (uint i = 0; i < 4; i++) {
        //    printf("    V[%i] = (%f,%f,%f)\n", i, positions[i].x, positions[i].y, positions[i].z);
        //}

        //assert(point_inside(circum, centroid));
        //assert(point_inside(volume, cell_handle, centroid));

        if (cell_handle.id >= circumspheres.length) {
            uint capacity = max(cell_handle.id+1, circumspheres.capacity * 2);
            circumspheres.reserve(capacity);
            circumspheres.length = capacity;
        }
        circumspheres[cell_handle.id] = circum;

        compute_normals(volume.vertices, cell);

        return true;
    }
    
    bool add_vertex(vertex_handle vert, uint depth = 0) {

        vec3 position = volume.vertices[vert.id].position;

        if (!find_in_circum(position)) return false;
        
        shared_face.capacity = cavity.length * 3;
        assert(shared_face.capacity < max_shared_face);
        shared_face.clear();
        
        printf("======== Retriangulating with %i faces\n", cavity.length);

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
                TriangleFaceSet face(cavity.verts);
                CFDCell& neighbor = volume.cells[cavity.handle.id];
                bool found_neighbor = false;
                for (uint i = 0; i < 4; i++) {
                    vertex_handle verts[3];
                    for (uint j = 0; j < 3; j++) {
                        verts[j] = neighbor.vertices[tetra_shape.faces[i].verts[j]];
                    }

                    if (face == verts) {
                        found_neighbor = true;
                        neighbor.faces[i].neighbor = free_cell_handle;
                        //printf("Connecting with neighbor %i [%i]\n", cavity.handle.id, i);
                        break;
                    }
                }
                //assert(found_neighbor);
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

            float r = circumsphere(points).radius;
            /*if (r > 50) {
                printf("Element is too bad!\n %i\n", free_cell_handle.id);
                circumspheres[free_cell_handle.id].radius = 0;
                free_cells.append(free_cell_handle);
                continue;
            }*/
            
            if (!update_circum(free_cell_handle)) return false;
            last = free_cell_handle;
        }

        //assert(free_cells.length == 0);

        return true;
    }
};

#include <algorithm>

void build_deluanay(CFDVolume& volume, slice<vertex_handle> verts, slice<Boundary> boundary) {
	//generate super triangle
	vec3 centroid;
	for (vertex_handle vertex : verts) {
		centroid += volume.vertices[vertex.id].position / (float)verts.length;
	}

	float r = 0.0f;
	for (vertex_handle vertex : verts) {
		//doesn't work for distances smaller than 1.0
		r = fmaxf(r, length(volume.vertices[vertex.id].position - centroid));
	}

	float l = sqrtf(24)*r; //Super equilateral tetrahedron side length
	float b = -r;
	float t = sqrtf(6)/3*l - r; 
	float e = sqrtf(3)/3*l;
	float a = M_PI * 2 / 3;

	int super_vert_offset = volume.vertices.length;
	int super_cell_offset = volume.cells.length;

    volume.vertices.append({ centroid + vec3(e * cosf(2 * a), b, e * sinf(2 * a)) });
    volume.vertices.append({ centroid + vec3(e * cosf(1 * a), b, e * sinf(1 * a)) });
	volume.vertices.append({ centroid + vec3(e * cosf(0 * a), b, e * sinf(0 * a)) });

	volume.vertices.append({ centroid + vec3(0, t, 0) });

	CFDCell cell{CFDCell::TETRAHEDRON};
	vec3 positions[4];

	for (int i = 0; i < 4; i++) {
		cell.vertices[i] = { super_vert_offset + i };
		positions[i] = volume.vertices[super_vert_offset + i].position;
	}

	Circumsphere circum = circumsphere(positions);

	int cell_id = volume.cells.length;
	volume.cells.append(cell);
    
    LinearAllocator& allocator = get_temporary_allocator();
    
    constexpr uint max_shared_faces = 10000;
    hash_map_base<TriangleFaceSet, FaceInfo> shared_faces = {
        max_shared_faces,
        alloc_t<hash_meta>(allocator, max_shared_faces),
        alloc_t<TriangleFaceSet>(allocator, max_shared_faces),
        alloc_t<FaceInfo>(allocator, max_shared_faces)
    };

    BowyerWatson bowyer{volume, shared_faces, max_shared_faces, super_cell_offset};
    bowyer.last = {cell_id};
    bowyer.update_circum({ cell_id });

    /*std::sort(verts.begin(), verts.end(), [&](vertex_handle a, vertex_handle b) {
        vec3 p_a = volume.vertices[a.id].position;
        vec3 p_b = volume.vertices[b.id].position;
        return p_a.x < p_b.x;
    });*/

    //4136
    /*for (uint i = 0; i < verts.length; i++) {        
        assert(verts[i].id < volume.vertices.length);
        vec3 p = volume.vertices[verts[i].id].position;
        assert(point_inside(volume, { super_cell_offset }, p));

    }*/

    for (uint i = 0; i < verts.length; i++) {
        //printf("Adding vertex %i\n", i);
        clear_debug();
        if (!bowyer.add_vertex(verts[i])) {
            vec3 pos = volume.vertices[verts[i].id].position;
            add_box_cell(volume, pos, 0.1);
            draw_debug_boxes(volume);
            break;
        }
        if (i == 2) return;
    }

	//Cleanup
	for (uint i = super_cell_offset; i < volume.cells.length; i++) {
		CFDCell& cell = volume.cells[i];
		uint shared = 0;
		for (uint j = 0; j < 4; j++) {
			uint index = cell.vertices[j].id;
			if (index >= super_vert_offset && index < super_vert_offset + 4) {
				shared++;
				break;
			}
		}

		if (shared >= 1) {
			for (uint j = 0; j < 4; j++) cell.vertices[j].id = 0;
		}
	}

	//refine
	/*
	uint n = 3;
	for (uint i = 0; i < n; i++) {
		for (uint i = super_cell_offset; i < volume.cells.length; i++) {
			CFDCell& cell = volume.cells[i];
			vec3 positions[4];

			get_positions(volume.vertices, { cell.vertices,4 }, positions);
			float skewness = tetrahedron_skewness(positions);

			if (skewness > 0.5) {
				vec3 circum_center = circumsphere(positions);

				vertex_handle v = { (int)volume.vertices.length };
				volume.vertices.append({ circum_center });
				add_vertex(volume, acceleration, shared_face, in_circum, v);
			}
		}
	}
	*/

}
