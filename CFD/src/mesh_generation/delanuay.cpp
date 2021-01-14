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

bool point_inside(Circumsphere& sphere, vec3 point) {
    return sq_distance(point, sphere.center)+FLT_EPSILON < sphere.radius*sphere.radius;
}

#define MATRIX_CIRCUM_IMPL

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

	if (fabs(det) < 0.001) {
		//printf("Co-planar points\n");
		c = (p[0] + p[1] + p[2] + p[3]) / 4;
	//return vec4((p[0]+p[1]+p[2]+p[3])/3, length(p[0]-p[1]));
	}

	float r = length(c - p[0]);

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

struct FaceInfo {
    uint count = 0;
    uint face;
    cell_handle cells[2];
};

struct CavityFace {
    cell_handle handle;
    vertex_handle verts[3];
};

struct BowyerWatson {
    CFDVolume& volume;
    hash_map_base<TriangleFaceSet, FaceInfo> shared_face;
    uint max_shared_face;
    tvector<CavityFace> cavity;
    tvector<Circumsphere> circumspheres;
    tvector<cell_handle> stack;
    tvector<cell_handle> free_cells;
    cell_handle last;
    
    void find_in_circum(vec3 pos) {
        //Walk structure to get to enclosing triangle, starting from the last inserted
        cell_handle current = last;
        
        float min_dist = sq_distance(circumspheres[current.id].center, pos);
        
        cell_handle closest_neighbor;
        do {
            auto& faces = volume.cells[current.id].faces;
            
            closest_neighbor = {};
            
            for (uint i = 0; i < 4; i++) {
                cell_handle neighbor = faces[i].neighbor;
                if (neighbor.id == -1) continue;
                float dist = length(circumspheres[neighbor.id].center - pos); //todo avoid sqrtf
                if (dist < min_dist) {
                    closest_neighbor = neighbor;
                    min_dist = dist;
                }
            }
        } while (closest_neighbor.id != -1);
        
        stack.clear();
        cavity.clear();
        
        stack.append(current);
        free_cells.append(current);
        
        while (stack.length > 0) {
            current = stack.pop();
            CFDCell& cell = volume.cells[current.id];
            
            for (uint i = 0; i < 4; i++) {
                vertex_handle verts[3];
                for (uint j = 0; j < 3; j++) {
                    verts[j] = cell.vertices[tetra_shape.faces[i].verts[2 - j]];
                }
                
                cell_handle neighbor = cell.faces[i].neighbor;
                
                if (neighbor.id != -1) {
                    bool visited = circumspheres[neighbor.id].radius == 0;
                    bool inside = visited || point_inside(circumspheres[neighbor.id], pos);
                    
                    if (!visited && inside) {
                        circumspheres[neighbor.id].radius = 0; //Mark for deletion and that it is already visited
                        stack.append(neighbor);
                        free_cells.append(current);
                    }
                    
                    if (!inside) { //face is unique
                        cavity.append({current, {verts[0], verts[1], verts[2]}});
                    }
                } else {
                    cavity.append({current, {verts[0], verts[1], verts[2]}}); //Super-triangle
                }
            }
        }
    }
    
    void update_circum(cell_handle cell_handle) {
        vec3 positions[4];
        get_positions(volume.vertices, { volume.cells[cell_handle.id].vertices, 4 }, positions);

        Circumsphere circum = circumsphere(positions);
        
        if (cell_handle.id >= circumspheres.length) {
            uint capacity = max(cell_handle.id+1, circumspheres.capacity * 2);
            circumspheres.reserve(capacity);
            circumspheres.length = capacity;
        }
        circumspheres[cell_handle.id] = circum;
    }
    
    bool add_vertex(vertex_handle vert, uint depth = 0) {
        vec3 position = volume.vertices[vert.id].position;

        find_in_circum(position);
        
        shared_face.capacity = cavity.length * 3;
        assert(shared_face.capacity < max_shared_face);
        shared_face.clear();
        
        assert(cavity.length >= 3);
        
        printf("Retriangulating with %i faces\n", cavity.length);

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
            free_cell.faces[0].neighbor = cavity.handle;
            
            for (uint i = 1; i < 4; i++) {
                vertex_handle verts[3];
                for (uint j = 0; j < 3; j++) {
                    verts[j] = free_cell.vertices[tetra_shape.faces[i].verts[j]];
                }
                
                FaceInfo& info = shared_face[verts];
                if (info.count++ == 0) {
                    info.cells[0] = free_cell_handle;
                    info.face = i;
                } else {
                    cell_handle neighbor = info.cells[0];
                    volume.cells[neighbor.id].faces[info.face].neighbor = free_cell_handle;
                }
            }
            
            update_circum(free_cell_handle);
            last = free_cell_handle;
        }

        return true;
    }
};

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

	volume.vertices.append({ centroid + vec3(e * cosf(0 * a), b, e * sinf(0 * a)) });
	volume.vertices.append({ centroid + vec3(e * cosf(1 * a), b, e * sinf(1 * a)) });
	volume.vertices.append({ centroid + vec3(e * cosf(2 * a), b, e * sinf(2 * a)) });
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
    
    constexpr uint max_shared_faces = 1000;
    hash_map_base<TriangleFaceSet, FaceInfo> shared_faces = {
        max_shared_faces,
        alloc_t<hash_meta>(allocator, max_shared_faces),
        alloc_t<TriangleFaceSet>(allocator, max_shared_faces),
        alloc_t<FaceInfo>(allocator, max_shared_faces)
    };

    BowyerWatson bowyer{volume, shared_faces, max_shared_faces};
    bowyer.last = {cell_id};
    bowyer.update_circum({cell_id});

    for (uint i = 0; i < verts.length; i++) {
        printf("Adding vertex %i\n", i);
        bowyer.add_vertex(verts[i]);
        
    }

	//Cleanup
	for (uint i = super_cell_offset; i < volume.cells.length; i++) {
		CFDCell& cell = volume.cells[i];
		bool destroy = false;
		for (uint j = 0; j < 4; j++) {
			uint index = cell.vertices[j].id;
			if (index >= super_vert_offset && index < super_vert_offset + 4) {
				destroy = true;
				break;
			}
		}

		if (destroy) {
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
