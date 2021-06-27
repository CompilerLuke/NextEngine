#include "core/math/vec4.h"
#include "core/math/vec3.h"
#include <glm/mat3x3.hpp>
#include "core/memory/allocator.h"
#include "core/container/tvector.h"
#include "core/container/hash_map.h"
#include "core/algorithm.h"
#define _USE_MATH_DEFINES
#include <math.h>

#include "core/math/aabb.h"
#include "mesh.h"
#include "mesh/vert_graph.h"

#include "geo/predicates.h"
#include "mesh_generation/delaunay.h"

#include "visualization/debug_renderer.h"

#include <unordered_map>

#include "mesh/input_mesh_bvh.h"
#include "editor/selection.h"
#include "core/math/intersection.h"

struct CavityFace {
	tet_handle tet;
	vertex_handle verts[3];
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

struct Delaunay {
	//Delaunay
	CFDVolume& volume;
	vector<CFDVertex>& vertices;
	CFDDebugRenderer& debug;
	std::unordered_map<TriangleFaceSet, tet_handle> shared_face;
	//hash_map_base<TriangleFaceSet, tet_handle> shared_face;
	AABB aabb;
	uint max_shared_face;
	uint super_vert_offset;
	uint boundary_vert_offset;
	vector<CavityFace> cavity;
	tvector<tet_handle> stack;
	tet_handle last;
	vector<tet_handle> free;

	//Mesh
	uint tet_capacity;
	//Tet* tets;
	vertex_handle* indices;
	tet_handle* faces;
	float* subdets;
	uint tet_count;

	//Advancing front
	//tet_handle active_faces;
	VertexInfo* active_verts;
	uint active_vert_offset;
	uint prev_active_vert_offset;
	tvector<vertex_handle> insertion_order;
	tvector<tet_handle> active_faces;
	uint grid_vert_begin;
	uint grid_vert_end;
	tet_handle active_tet;

	float min_quad_quality;
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
	return length(point - sphere.center) + bias < sphere.radius;
}

real orient3d(vec3 a, vec3 b, vec3 c, vec3 d) {
	return orient3d(&a.x, &b.x, &c.x, &d.x);
}

real l1_norm(vec3 a, vec3 b) {
	return max(abs(a - b));
}

real l1_norm_sq(vec3 a, vec3 b) {
	real d = l1_norm(a, b);
	return d * d;
}

Circumsphere circumsphere(vec3 p[4]) {
	vec3 u1 = p[0] - p[3];
	vec3 u2 = p[1] - p[3];
	vec3 u3 = p[2] - p[3];

#if 0
	vec3 center = (l1_norm_sq(p[0], p[3]) * cross(u2, u3)
		+ l1_norm_sq(p[1], p[3]) * cross(u3, u1)
		+ l1_norm_sq(p[2], p[3]) * cross(u1, u2))
		/ (2 * dot(u1, cross(u2, u3)));
	center += p[3];
	float r = l1_norm(center, p[0]);
#else
	float det = 2 * dot(u1, cross(u2, u3));
	//if (fabs(det) < 0.001) return { 0, 0 };
	vec3 center = (sq_distance(p[0], p[3]) * cross(u2, u3)
		+ sq_distance(p[1], p[3]) * cross(u3, u1)
		+ sq_distance(p[2], p[3]) * cross(u1, u2))
		/ det;
	center += p[3];
	float r = length(center - p[0]);
#endif
	return { center, r };
}

// hextreme
// see Perturbations and Vertex Removal in a 3D Delaunay Triangulation, O. Devillers & M. Teillaud
static inline real symbolic_perturbation(vertex_handle indices[5], vec3 i, vec3 j, vec3 k, vec3 l, vec3 m) {

	const vec3* pt[5] = { &i,&j,&k,&l,&m };

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
			if (indices[i].id > indices[i + 1].id) {

				const vec3* swappt = pt[i];
				pt[i] = pt[i + 1];
				pt[i + 1] = swappt;

				vertex_handle sw = indices[i];
				indices[i] = indices[i + 1];
				indices[i + 1] = sw;
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
	float* subdet = d.subdets + curr;

#if 1
	vec3 p[5];
	for (uint i = 0; i < 4; i++) p[i] = d.vertices[d.indices[curr + i].id].position;
	p[4] = d.vertices[v.id].position;

	real det = insphere(&p[0].x, &p[1].x, &p[2].x, &p[3].x, &p[4].x);

	if (det == 0.0) {
		vertex_handle nn[5];
		memcpy_t(nn, d.indices + curr, 4);
		nn[4] = v;
		//NE_INFO("symbolic perturbation");
		det = symbolic_perturbation(nn, p[0], p[1], p[2], p[3], p[4]);
	}

	return det;


#else

	vec3 a = mesh[cell.vertices[0]].position;
	vec3 e = mesh[v].position;
	vec3 ae = e - a;

	if (cell.vertices[3].id == -1) {
		real det = dot(ae, subdet);

		if (fabs(det) > o3dstaticfilter) return det;

		vec3 b = mesh.vertices[cell.vertices[1].id].position;
		vec3 c = mesh.vertices[cell.vertices[2].id].position;

		det = orient3d(a, b, c, e);

		if (det != 0.0) return det;

		// we never go here, except when point are aligned on boundary
		// NE_INFO("insphere using opposite vertex");
		CFDCell& opposite_cell = mesh[cell.faces[3].neighbor];
		vertex_handle opposite_v = opposite_cell.vertices[0];
		vec3 opposite_vertex = mesh[opposite_v].position;
		det = -insphere(&a.x, &b.x, &c.x, &opposite_vertex.x, &e.x);

		if (det == 0.0) {
			vertex_handle nn[5] = { cell.vertices[0],cell.vertices[1],cell.vertices[2],opposite_v,v };
			NE_INFO("symbolic perturbation on boundary");
			det = -symbolicPerturbation(nn, a, b, c, opposite_vertex, e);
			if (det == 0.0) NE_WARNING("Symbolic perturbation failed\n");
		}
		return det;
	}

	real aer = dot(ae, ae);
	real det = ae.x * subdet.x - ae.y * subdet.y + ae.z * subdet.z - aer * subdet.w;

	if (fabs(det) > ispstaticfilter) return det;

	vec3 b = mesh[cell.vertices[1]].position;
	vec3 c = mesh[cell.vertices[2]].position;
	vec3 d = mesh[cell.vertices[3]].position;

	det = insphere(&a.x, &b.x, &c.x, &d.x, &e.x);

	if (det == 0.0) {
		vertex_handle nn[5];
		memcpy_t(nn, cell.vertices, 4);
		nn[4] = v;
		NE_INFO("symbolic perturbation");
		det = symbolicPerturbation(nn, a, b, c, d, e);
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
		vec3 position = d.vertices[indices[tet + i].id].position;
		centroid += position / 4.0f;
	}

	vec4 ab;
	vec4 ac;

	float* subdet = d.subdets + tet;

	vec3 a = vertices[indices[tet + 0].id].position;
	vec3 b = vertices[indices[tet + 1].id].position;
	vec3 c = vertices[indices[tet + 2].id].position;

	if (indices[tet + 3].id != -1) {
		vec3 d = vertices[indices[tet + 3].id].position;
		vec4 ad;

		uint i;
		for (i = 0; i < 3; i++) {
			ab[i] = b[i] - a[i]; //AB
			ac[i] = c[i] - a[i]; //AC
			ad[i] = d[i] - a[i]; //AD
		}

		ab.w = dot(ab, ab);
		ac.w = dot(ac, ac);
		ad.w = dot(ad, ad);

		real cd12 = ac.z * ad.y - ac.y * ad.z;
		real db12 = ad.z * ab.y - ad.y * ab.z;
		real bc12 = ab.z * ac.y - ab.y * ac.z;

		real cd30 = ac.x * ad.w - ac.w * ad.x;
		real db30 = ad.x * ab.w - ad.w * ab.x;
		real bc30 = ab.x * ac.w - ab.w * ac.x;

		// each subdet is a simple triple product
		subdet[0] = ab.w * cd12 + ac.w * db12 + ad.w * bc12;
		subdet[1] = ab.z * cd30 + ac.z * db30 + ad.z * bc30;
		subdet[2] = ab.y * cd30 + ac.y * db30 + ad.y * bc30;
		subdet[3] = ab.x * cd12 + ac.x * db12 + ad.x * bc12;

		// SubDet[3]=AD*(ACxAB) = 6 X volume of the tet !
	}
	else {
		uint i;
		for (i = 0; i < 3; i++) {
			ab[i] = b[i] - a[i]; //AB
			ac[i] = c[i] - a[i]; //AC
		}

		vec3 c = cross(ac, ab);
		*(vec4*)subdet = vec4(c, dot(c, c));
	}
}

float tetrahedron_skewness(vec3 positions[4]) {
	float min_length = FLT_MAX;
	float max_length = 0;
	for (uint i = 0; i < 4; i++) {
		for (uint j = i + 1; j < 4; j++) {
			float l = length(positions[i] - positions[j]);
			min_length = fminf(min_length, l);
			max_length = fmaxf(max_length, l);
		}
	}
	return (max_length - min_length) / max_length;
}

bool point_inside(Delaunay& d, tet_handle tet, vec3 pos) {
	for (uint i = 0; i < 4; i++) {
		vec3 verts[3];
		for (uint j = 0; j < 3; j++) {
			verts[j] = d.vertices[d.indices[tet + tetra_shape[i][j]].id].position;
		}

		vec3 n = triangle_normal(verts);
		vec3 p = d.vertices[d.indices[tet + tetra_shape[i][0]].id].position;
		float d = dot(n, pos - p);
		if (d > 3 * FLT_EPSILON) {
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
	debug_box.append({ p - s, p + s });
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
		centroid += d.vertices[d.indices[tet + i].id].position;
	}
	centroid /= 4;
	return centroid;
}

void get_positions(Delaunay& d, tet_handle tet, uint face, vec3 verts[3]) {
	for (uint i = 0; i < 3; i++) {
		verts[i] = d.vertices[d.indices[tet + tetra_shape[face][i]].id].position;
	}
}

tet_handle find_enclosing_tet(Delaunay& d, vec3 pos, float min_dist = 0.0, bool stop_at_boundary = false) {
	//Walk structure to get to enclosing triangle, starting from the last inserted
	tet_handle current = d.last;
	tet_handle prev = UINT_MAX;

	vec3 last_pos = compute_centroid(d, current);
	//printf("==============\n");
	//printf("Starting at %i\n", current.id);

	//clear_debug();
	//add_box_cell(d.volume, pos, vec3(3));

	uint count = 0;
loop: {
	uint offset = rand();

	/*if (stop_at_boundary) {
		vec3 current_pos = compute_centroid(d, current);
		draw_line(d.debug, last_pos, current_pos, vec4(0, 1, 0, 1));
		last_pos = current_pos;
	}*/

	for (uint it = 0; it < 4; it++) {
		uint i = (it + offset) % 4;
		tet_handle neighbor = d.faces[current + i] & NEIGH;
		if ((!neighbor && !stop_at_boundary) || neighbor == prev) continue;
		
		vec3 verts[3];
		get_positions(d, current, i, verts);
		
		float epsilon = neighbor == 0 ? -0.2 : 0;

		/*if (!neighbor) {
			draw_triangle(d.debug, verts, vec4(1, 1, 1, 1));
			draw_triangle(d.debug, verts[2], verts[1], verts[0], vec4(1, 0, 0, 1));
		}*/

		//vec3 n = triangle_normal(verts);

		real det =
			//dot(n, pos - verts[0]);
			-orient3d(verts[0], verts[1], verts[2], pos);
		if (det > epsilon) {
			if (neighbor == 0) return 0;
			//vec3 start = compute_centroid(d, current);
			//vec3 end = compute_centroid(d, neighbor);

			//draw_ray(d.volume, start, normalize(end-start), length(end-start));

			last_pos = (verts[0] + verts[1] + verts[2]) / 3.0f;

			prev = current;
			current = neighbor;
			if (count++ > 1000) return 0;
			goto loop;
		}
	}
	};

	//if (stop_at_boundary) draw_line(d.debug, last_pos, pos, vec4(0, 0, 1, 1));

	if (false && min_dist != 0.0f) {
		for (uint i = 0; i < 4; i++) {
		vec3 vert = d.vertices[d.indices[current + i].id].position;
		if (length(vert - pos) < min_dist) {
			return 1;
		}
	}
}

return current;
}

bool is_deallocated(Delaunay& d, tet_handle tet) {
	return d.subdets[(tet & NEIGH) + 3] == -1;
}

void dealloc_tet(Delaunay& d, tet_handle tet) {
	d.subdets[(tet & NEIGH) + 3] = -1; //Mark for deletion and that it is already visited
	d.free.append(tet);
}

bool is_sliver(Delaunay& d, tet_handle tet) {
	float highest_volume = 0;

	tet = tet & NEIGH;

	vec3 tet_verts[4];
	for (uint i = 0; i < 4; i++) tet_verts[i] = d.vertices[d.indices[tet + i].id].position;

	
	for (uint i = 0; i < 4; i++) {
		vec3 verts[4];
		for (uint j = 0; j < 4; j++) verts[j] = tet_verts[tetra_shape[i][j]];

		/*vec3 ab = verts[1] - verts[0];
		vec3 ac = verts[2] - verts[0];
		vec3 ad = verts[3] - verts[0];*/

		float volume = fabs(orient3d(verts[0], verts[1], verts[2], verts[3]));
		//fabs(dot(normalize(ad), normalize(cross(ac, ab))));
		highest_volume = fmaxf(highest_volume, fabs(volume));
	}
	

	return highest_volume < 0.15;

	//return fabs(orient3d(tet_verts[0], tet_verts[1], tet_verts[2], tet_verts[3])) < 0.01;

	/*
	vec3 verts[4];
	for (uint i = 0; i < 4; i++) verts[i] = d.vertices[d.indices[tet + i].id].position;

	//return fabs(orient3d(verts[0], verts[1], verts[2], verts[3])) < 0.1;

	float l = 0.0f; // FLT_MAX;
	float sl = FLT_MAX;
	uint count = 0;
	for (uint i = 0; i < 4; i++) {
		for (uint j = i + 1; j < 4; j++) {
			float edge = length(verts[i] - verts[j]);
			l += edge; // fmaxf(l, edge);
			sl = fminf(sl, edge);
			count++;
		}
	}

	l /= 6.0f;
	//l = sqrtf(l);

	vec3 ab = verts[1] - verts[0];
	vec3 ac = verts[2] - verts[0];
	vec3 ad = verts[3] - verts[0];

	float volume = dot(ad, -cross(ac, ab)); // / 6.0f;
	//float radius = circumsphere(verts).radius;
	float p = l / sl;

	float vl = volume / (3*sl);
	
	const float p0 = 3.0;
	const float vl0 = 0.2;

	//p <= p0 &&
	return p <= p0 && vl <= vl0;*/
}

int find_cavity_faces(Delaunay& d, tet_handle current, vertex_handle v, float min_dist = 0.0f) {
	//printf("Inserting %i,  (%f, %f, %f)\n", v.id, pos.x, pos.y, pos.z);

	//if (!point_inside(d, current, pos)) return false;
	//assert(insphere(d, current, v) < 0);

	d.cavity.length = 0;
	d.stack.append(current);
	dealloc_tet(d, current);

	vec3 v_pos = d.vertices[v.id].position;
	vec3 last_pos = compute_centroid(d, current);

	while (d.stack.length > 0) {
		tet_handle current = d.stack.pop();
		vec3 current_pos = compute_centroid(d, current);

		if (min_dist == 1) {
			draw_line(d.debug, last_pos, current_pos, vec4(1, 0, 0, 1));
			last_pos = current_pos;
		}

		//Tet& cell = d.tets[current];
		//stack = cell.prev;

		//printf("Visiting %i\n", current);
		for (uint i = 0; i < 4; i++) {
			tet_handle neighbor = d.faces[current + i] & NEIGH;

			vertex_handle verts[3];
			for (uint j = 0; j < 3; j++) {
				verts[j] = d.indices[current + tetra_shape[i][2 - j]];
				//Reverse orientation, as tetrashape will reverse orientation when extruding
				//Reverse-reverse -> no face flip -> as face should point outwards
			}

			if (neighbor) {
				bool visited = is_deallocated(d, neighbor);
				if (visited) continue;

				bool inside = insphere(d, neighbor, v) > 0;
				if (inside) {
					//printf("Neighbor %i: adding to stack\n", neighbor.id);
					dealloc_tet(d, neighbor);
					d.stack.append(neighbor);
				}

				//printf("Face %i, neighbor %i, %s\n", i, neighbor.id, inside ? "not unique" : "unique");
				else { //face is unique
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
		CFDDebugRenderer& debug = d.debug;
		for (CavityFace face : d.cavity) {
			vec3 pos[3];
			get_positions(d.vertices, { face.verts,3 }, pos);

			//draw_triangle(debug, pos, vec4(1,0,0,1));
		}
		suspend_execution(debug);
		printf("Cavity does not enclose point\n");
	}
	return d.cavity.length > 3;
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
	tet_handle tet;
	if (d.free.length > 0) {
		tet = d.free.pop();
	}
	else if (d.tet_count < d.tet_capacity) {
		tet = 4 * d.tet_count++;
	}
	else {
		uint prev_count = d.tet_capacity;
		uint tet_count = max(prev_count * 2, 4048);

		u64 elem = (sizeof(Tet) + sizeof(vertex_handle) + sizeof(tet_handle) + sizeof(float)) * 4;

		//d.tets = (Tet*)realloc(d.tets, sizeof(Tet)*4*tet_count);
		d.indices = (vertex_handle*)realloc(d.indices, sizeof(vertex_handle) * 4 * tet_count);
		d.faces = (tet_handle*)realloc(d.faces, sizeof(tet_handle) * 4 * tet_count);
		d.subdets = (float*)realloc(d.subdets, sizeof(float) * 4 * tet_count);

		printf("Reallocating tet count: %i\n", tet_count);

		/*d.tets = (Tet*)realloc(d.tets, size);
		d.indices = (vertex_handle*)((char*)d.tets + 4*sizeof(Tet)*tet_count);
		d.faces = (tet_handle*)((char*)d.indices + 4*sizeof(vertex_handle) * tet_count);
		d.subdets = (float*)((char*)d.faces + 4*sizeof(tet_handle) * tet_count);*/

		//memset(d.tets + 4*prev_count, 0, 4*sizeof(float)*(tet_count-prev_count));
		memset(d.indices + 4 * prev_count, 0, 4 * sizeof(vertex_handle) * (tet_count - prev_count));

		//d.free_chain = start + 4;
		d.tet_capacity = tet_count;
		tet = 4 * d.tet_count++;
	}
	//d.tets[tet] = {};
	d.subdets[tet + 3] = 0;
	d.last = tet;

	d.active_tet = min(d.active_tet, tet);

	return tet;
}

bool is_super_vert(Delaunay& d, vertex_handle v) {
	return v.id >= d.super_vert_offset && v.id < d.super_vert_offset + 4;
}

bool is_boundary_vert(Delaunay& d, vertex_handle v) {
	return v.id < d.boundary_vert_offset;
}

bool is_grid_vert(Delaunay& d, vertex_handle v) {
	return v.id >= d.grid_vert_begin && v.id < d.grid_vert_end;
}

bool is_active_vert(Delaunay& d, vertex_handle v) {
	return v.id >= d.active_vert_offset && !is_super_vert(d, v);
}

bool is_or_was_active_vert(Delaunay& d, vertex_handle v) {
	return v.id >= d.prev_active_vert_offset && !is_super_vert(d, v);
}

bool retriangulate_cavity(Delaunay& d, slice<CavityFace> cavity, vertex_handle vert) {
	/*d.shared_face.capacity = cavity.length * 4;
	if (d.shared_face.capacity > d.max_shared_face) {
		printf("Cavity is too large!!\n");
		return false;
	}
	*/
	d.shared_face.clear();

	//printf("======== Retriangulating with %i faces\n", d.cavity.length);

	for (CavityFace& cavity : cavity) {
		tet_handle tet = alloc_tet(d);
		//printf("Alloc   %i\n", tet);

		memcpy_t(d.indices + tet, cavity.verts, 3);
		d.indices[tet + 3] = vert;

		vec3 centroid;
		for (uint i = 0; i < 4; i++) {
			centroid += d.vertices[d.indices[tet + i].id].position;
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
				verts[j] = d.indices[tet + tetra_shape.faces[i].verts[j]];
			}

			tet_handle face = tet + i;
			tet_handle& neigh = d.shared_face[verts];
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

	return true;
}

bool add_vertex(Delaunay& d, vertex_handle vert, float min_dist = 0.0f) {
	vec3 pos = d.vertices[vert.id].position;
	tet_handle current = find_enclosing_tet(d, pos, min_dist, false);
	if (!current) {
		draw_line(d.debug, pos, pos + vec3(0, 10, 0), vec4(0, 1, 0, 1));
		suspend_execution(d.debug);
		printf("Could not find enclosing tet!\n");
		return false;
	}
	if (current == 1) return 2; //too close
	
	uint res = find_cavity_faces(d, current, vert, min_dist);
	if (res == 2) return true; //too close
	if (res == 0) return false; //failed

	retriangulate_cavity(d, d.cavity, vert);
	//assert(free_cells.length == 0);

	return true;
}

void start_with_non_coplanar(CFDVolume& mesh, slice<vertex_handle> vertices) {
	uint i = 0, j = 1, k = 2, l = 3;
	uint n = vertices.length;
	bool coplanar = true;
	real orientation;
	for (i = 0; coplanar && i < n - 3; i++) {
		for (j = i + 1; coplanar && j < n - 2; j++) {
			for (k = j + 1; coplanar && k < n - 1; k++) {
				for (l = k + 1; coplanar && l < n; l++) {
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


	if (orientation == 0.0) {
		NE_WARNING("all vertices are coplanar");
	}

	// swap 0<->i  2<->j 2<->k 3<->l


	std::swap(mesh.vertices[0], mesh.vertices[i]);
	std::swap(mesh.vertices[1], mesh.vertices[j]);
	std::swap(mesh.vertices[2], mesh.vertices[k]);
	std::swap(mesh.vertices[3], mesh.vertices[l]);
}

void brio_vertices(CFDVolume& mesh, const AABB& aabb, slice<vertex_handle> vertices);

#include <queue>
tvector<Boundary> quad_to_tri_boundary(slice<Boundary> boundaries) {
    tvector<Boundary> tri_boundaries;
    
    for (Boundary b : boundaries) {
        bool is_quad = b.vertices[3].id != -1;
        tri_boundaries.append({ b.cell, { b.vertices[0], b.vertices[1], b.vertices[2] } });
        if (is_quad) {
            tri_boundaries.append({ b.cell, { b.vertices[0], b.vertices[2], b.vertices[3] } });

            tri_boundaries.append({ b.cell, { b.vertices[0], b.vertices[1], b.vertices[3] } }); //Alternative triangulation of quad
            tri_boundaries.append({ b.cell, { b.vertices[1], b.vertices[2], b.vertices[3] } });
        }
    }
    
    return tri_boundaries;
}

void identify_boundary_faces(Delaunay& d, slice<Boundary> tri_boundaries, uint* boundary) {
    for (Boundary tri : tri_boundaries) {
        vec3 pos[3];
        get_positions(d.vertices, { tri.vertices, 3 }, pos);

        vec3 normal = triangle_normal(pos);
        vec3 center = triangle_center(pos);

        //draw_line(debug, center, center + normal*0.2, vec4(0, 0, 0, 1));

        vec3 search_pos = center + normal * 0.2;

        TriangleFaceSet triangle(tri.vertices);

        tet_handle boundary_tet = find_enclosing_tet(d, search_pos);

        if (!boundary_tet) {
            printf("Could not find boundary tet!\n");
            continue;
        }
        d.last = boundary_tet;

        float smallest_disp = FLT_MAX;
        
        bool found = true;
        for (uint i = 0; i < 4; i++) {
            vertex_handle verts[3];
            for (uint j = 0; j < 3; j++) {
                verts[j] = d.indices[boundary_tet + tetra_shape[i][j]];
            }

            /*for (uint j = 0; j < 3; j++) {
                vec3 pos0 = d.vertices[verts[j].id].position;
                vec3 pos1 = d.vertices[verts[(j + 1) % 3].id].position;

                draw_line(debug, pos0, pos1, vec4(0, 0, 0, 1));
            }*/

            TriangleFaceSet neigh_triangle(verts);

            if (triangle == neigh_triangle) {
                
                //draw_triangle(d.debug, pos, vec4(1));
                tet_handle face = boundary_tet + i;
                tet_handle neigh = d.faces[face];

                boundary[face / 32] |= 1 << (face % 32);
                boundary[neigh / 32] |= 1 << (neigh % 32);

                break;
            }
        }

    }
}

struct IdentifyBoundaryJob {
    InputModelBVH* bvh;
    Delaunay* d;
    uint* boundary;
    uint begin;
    uint end;
};

//todo doing double the work
void identify_boundary_faces_job(IdentifyBoundaryJob& job) {
    Delaunay& d = *job.d;
    InputModelBVH& bvh = *job.bvh;
    
    for (uint i = job.begin; i < job.end; i++) {
        tet_handle tet = i & NEIGH;
		tet_handle neigh = d.faces[i];

		bool tet_is_sliver = is_sliver(d, tet);
		bool neigh_is_sliver = is_sliver(d, neigh & NEIGH);

		//if (tet_is_sliver) continue;
		if (neigh < tet) continue; //

        uint face = i & NEIGH_FACE;
        
        vec3 verts[3];
        get_positions(d, tet, face, verts);
        
        vec3 pos = triangle_center(verts);
        vec3 normal = triangle_normal(verts);

		//float size = (sq(verts[0] - verts[1]) + sq(verts[1] - verts[2]) + sq(verts[2] - verts[0])) / 3.0f;
        
		float threshold = 0.1; // fminf(size * 0.5, 0.1);
        
        vec3 p0 = pos - normal*threshold;
        vec3 p1 = pos + normal*threshold;
        
        float len = length(p1 - p0);
        
        Ray ray(p0, (p1 - p0) / len, len);
        InputModelBVH::RayHit hit;
        
        if (!bvh.ray_intersect(ray, glm::mat4(1.0), hit, MeshPrimitive::Triangle)) continue;
    
        //draw_triangle(d.debug, verts, vec4(1,0,0,1));
        
        job.boundary[i / 32] |= 1 << (i % 32);
        //job.boundary[neigh / 32] |= 1 << (neigh % 32);
        
        draw_triangle(d.debug, verts[2], verts[1], verts[0], vec4(1));
        draw_triangle(d.debug, verts[0], verts[1], verts[2], vec4(1));
    }
}

#include "core/job_system/job.h"


//todo doing double the work
void identify_boundary_faces(Delaunay& d, InputModelBVH& bvh, uint* boundary) {
    const uint MAX_WORKERS = 128;
    
    IdentifyBoundaryJob data[MAX_WORKERS];
    JobDesc jobs[MAX_WORKERS];
    
    uint begin = 0;
    uint end = 4*d.tet_count;
    
    uint num_chunks = divceil(end - begin, 32);
	uint num_jobs = 1; // 4 * worker_thread_count();
    uint chunks_per_job = divceil(num_chunks, num_jobs);
    
    printf("Launching jobs, chunks %i, jobs %i, chunks_per_job %i\n", num_chunks, num_jobs, chunks_per_job);
    
    for (uint i = 0; i < num_jobs; i++) {
        data[i].bvh = &bvh;
        data[i].d = &d;
        data[i].boundary = boundary;
        data[i].begin = max(begin + i*chunks_per_job*32, 4);
        data[i].end = min(begin + (i+1)*chunks_per_job*32, end);
        
        jobs[i] = JobDesc(identify_boundary_faces_job, data + i);
    }
    
    wait_for_jobs(PRIORITY_HIGH, {jobs, num_jobs});
    
    printf("Identified boundary\n");
	suspend_execution(d.debug);
}

void remove_tets_outside_boundary(Delaunay& d, uint* boundary) {
    uint* visited = TEMPORARY_ZEROED_ARRAY(uint, divceil(d.tet_count, 32));
    
    tet_handle outside_tet = 0;
    for (uint i = 4; i < d.tet_count * 4; i++) {
		if (is_deallocated(d, i)) continue;

        vertex_handle v = d.indices[i];
        if (is_super_vert(d, v)) {
            outside_tet = i & NEIGH;
            break;
        }
    }

    vec3 last = compute_centroid(d, outside_tet);

    dealloc_tet(d, outside_tet);

    //std::queue<tet_handle> queue;
    //queue.push(outside_tet);
    //tvector<tet_handle> stack;
    //stack.append(outside_tet);
    
    d.stack.append(outside_tet);
    visited[outside_tet/4] = 1ull << (outside_tet/4)%32;
    
    while (d.stack.length > 0) {
        uint data = d.stack.pop();
        
        tet_handle tet = data & NEIGH;
        bool is_tet_inside = data & NEIGH_FACE;
        
        vec3 current = compute_centroid(d, tet);
        //draw_line(d.debug, last, current, is_tet_inside ? vec4(0,1,0,1) : vec4(1,0,0,1));
        //suspend_execution(d.debug);
        last = current;

        for (uint i = 0; i < 4; i++) {
            uint face = tet + i;
            tet_handle neigh = d.faces[face];
            
            uint mask = 1ull << (neigh/4 % 32);
            
            bool is_boundary = boundary[face / 32] & (1 << (face % 32))
							 || boundary[neigh / 32] & (1 << (neigh % 32));

            if (is_boundary) {
                d.faces[neigh] = 0;
                d.faces[face] = 0;

                vec3 pos[3];
                get_positions(d, tet, i, pos);

                draw_triangle(d.debug, pos[2], pos[1], pos[0], vec4(1,0,0,1));
                draw_triangle(d.debug, pos[0], pos[1], pos[2], vec4(1,0,0,1));
            }
            
            ///32] & mask
            if (!neigh || visited[neigh/4/32] & mask) continue;
            visited[neigh/4/32] |= mask;
            
            bool is_neigh_inside = is_tet_inside ^ is_boundary;

            if (!is_neigh_inside) dealloc_tet(d, neigh & NEIGH);
            d.stack.append((neigh & NEIGH) | is_neigh_inside);
        }
    }
    
    suspend_execution(d.debug);
}

void draw_point(CFDDebugRenderer& debug, vec3 pos, vec4 color);

void identify_shadow_tets(Delaunay& d, uint shadow_watermark) {
	uint face_masks[4] = {};
	for (uint i = 0; i < 4; i++) {
		for (uint j = 0; j < 3; j++) {
			face_masks[i] |= 1 << tetra_shape[i][j];
		}
	}

	for (uint i = 4; i < d.tet_count * 4; i += 4) {
		if (is_deallocated(d, i)) continue;

		uint shadow_mask = 0;

		for (uint j = 0; j < 4; j++) {
			vertex_handle v = d.indices[i + j];
			if (v.id >= shadow_watermark && !is_super_vert(d,v)) {
				shadow_mask |= 1 << j;
				draw_point(d.debug, d.vertices[d.indices[i + j].id].position, vec4(0,0,0,1));
			}
		}

		if (!shadow_mask) continue;

		bool error = false;
		for (uint j = 0; j < 4; j++) {
			bool remove_face = face_masks[j] & shadow_mask;
			tet_handle neigh = d.faces[i + j];
			if (!neigh && !remove_face) error = true;
		}

		for (uint j = 0; j < 4; j++) {
			bool remove_face = face_masks[j] & shadow_mask;

			tet_handle neigh = d.faces[i + j];
			d.faces[neigh] = 0;

#if 1
			if (!error) continue;

			vec3 verts[3];
			get_positions(d, i, j, verts);
			
			if (!remove_face) {
				vec3 normal = triangle_normal(verts);
				//for (uint i = 0; i < 3; i++) verts[i] += 0.01 * normal;

				draw_triangle(d.debug, verts, RED_DEBUG_COLOR);
			}
			else {
				draw_triangle(d.debug, verts, vec4(1, 1, 1, 1));
			}
#endif
		}

		dealloc_tet(d, i);
	}

	d.vertices.length = shadow_watermark;

	suspend_execution(d.debug);
}

void constrain_triangulation(Delaunay& d, slice<Boundary> boundaries, uint shadow_watermark) {
	CFDDebugRenderer& debug = d.debug;

    uint* boundary = new uint[divceil(d.tet_count*4, 32)](); //todo check if overwritten and use temporary allocation
    //TEMPORARY_ZEROED_ARRAY(uint, divceil(d.tet_count*4, 32));

    tvector<Boundary> tri_boundaries = quad_to_tri_boundary(boundaries);
    identify_boundary_faces(d, tri_boundaries, boundary);
	remove_tets_outside_boundary(d, boundary);

	identify_shadow_tets(d, shadow_watermark);
    //identify_boundary_faces(d, bvh, boundary);
    

    delete boundary;
}

Delaunay* make_Delaunay(CFDVolume& volume, const AABB& aabb, CFDDebugRenderer& debug) {

	//Delaunay::Delaunay(CFDVolume& volume, const AABB& aabb) : vertices(volume.vertices), volume(volume) {
    
	Delaunay* d = PERMANENT_ALLOC(Delaunay, { volume, volume.vertices, debug });
	d->vertices = volume.vertices;
	d->max_shared_face = kb(64);
	d->aabb = aabb;
	d->tet_count = 1; //leave 0 as empty

	LinearAllocator& allocator = get_temporary_allocator();

	/*d->shared_face = {
		d->max_shared_face,
		alloc_t<hash_meta>(allocator, d->max_shared_face),
		alloc_t<TriangleFaceSet>(allocator, d->max_shared_face),
		alloc_t<tet_handle>(allocator, d->max_shared_face)
	};*/

	vec3 centroid = aabb.centroid();

	float m = 1.0;
	printf("MULTIPLIER %f\n", m);
	float r = m * length(aabb.max - centroid);


	float l = sqrtf(24) * r; //Super equilateral tetrahedron side length
	float b = -r;
	float t = sqrtf(6) / 3 * l - r;
	float e = sqrtf(3) / 3 * l;
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
		d->indices[super_tet + i] = { (int)(d->super_vert_offset + i) };
		d->faces[super_tet + i] = {};
		super_aabb.update(vertices[d->super_vert_offset + i].position);
	}

	exactinit(super_aabb.max.x - super_aabb.min.x, super_aabb.max.y - super_aabb.min.y, super_aabb.max.z - super_aabb.min.z); // for the predicates to work

	return d;
}

void remove_super(Delaunay& d) {
	for (tet_handle i = 4; i < 4 * d.tet_count; i += 4) {
		if (d.subdets[i + 3] == -1) continue;
		bool super = false;
		bool boundary = true;
		bool grid = true;
		
		for (uint j = 0; j < 4; j++) {
			vertex_handle v = d.indices[i + j];
			if (is_super_vert(d, v)) {
				super = true;
				break;
			}
			if (boundary && !is_boundary_vert(d, v)) boundary = false;
			if (grid && !is_grid_vert(d, v)) grid = false;
		}

		if (!(super || boundary)) continue;
		for (uint j = 0; j < 4; j++) {
			d.faces[d.faces[i + j]] = 0; //unlink
		}
		dealloc_tet(d, i);
	}
}

#include <algorithm>

void build_vertex_graph(Delaunay& d, VertGraph& graph) {
	VertGraph::Slice* id_to_neighbors = TEMPORARY_ZEROED_ARRAY(VertGraph::Slice, d.vertices.length);

	for (tet_handle i = 4; i < d.tet_count; i += 4) {
		//if (is_deallocated(d, i)) continue;
		for (uint a = 0; a < 4; a++) {
			for (uint b = a + 1; b < 4; b++) {
				id_to_neighbors[d.indices[i + a].id].count++;
				id_to_neighbors[d.indices[i + b].id].count++;
			}
		}
	}

	uint offset = 0;
	for (uint i = 0; i < d.vertices.length; i++) {
		uint count = id_to_neighbors[i].count;
		id_to_neighbors[i].count = 0;
		id_to_neighbors[i].offset = offset;
		offset += count;
	}

	uint neighbors_length = offset;
	vertex_handle* neighbors = TEMPORARY_ARRAY(vertex_handle, offset);

	for (tet_handle i = 4; i < d.tet_count; i += 4) {
		//if (is_deallocated(d, i)) continue;
		for (uint a = 0; a < 4; a++) {
			for (uint b = a + 1; b < 4; b++) {
				vertex_handle va = d.indices[i + a];
				vertex_handle vb = d.indices[i + b];

				VertGraph::Slice& neigh_a = id_to_neighbors[va.id];
				VertGraph::Slice& neigh_b = id_to_neighbors[vb.id];

				neighbors[neigh_a.offset + neigh_a.count++] = vb;
				neighbors[neigh_b.offset + neigh_b.count++] = va;
			}
		}
	}

	offset = 0;
	for (uint i = 0; i < d.vertices.length; i++) {
		VertGraph::Slice& neigh = id_to_neighbors[i];
		vertex_handle* connected = neighbors + neigh.offset;
		std::sort(connected, connected + neigh.count, [](vertex_handle a, vertex_handle b) {
			return a.id < b.id;
			});

		//compact
		uint prev_offset = offset;
		vertex_handle last = { 0 };
		for (uint i = 0; i < neigh.count; i++) {
			if (last.id != connected[i].id) {
				last = connected[i];
				neighbors[offset++] = last;
			}
		}

		neigh.offset = prev_offset;
		neigh.count = offset - prev_offset;
	}

	get_temporary_allocator().occupied -= (neighbors_length - offset) * sizeof(vertex_handle); //not very safe but should work

	graph.id_to_neighbors = id_to_neighbors;
	graph.neighbor_vertices = neighbors;
}

//Hex identification
bool is_face_corner(Delaunay& del, vertex_handle va, vertex_handle vb, vertex_handle vd) {
	vec3 a = del.vertices[va.id].position;
	vec3 b = del.vertices[vb.id].position;
	vec3 d = del.vertices[vd.id].position;

	//sin(acos(dot)) may give better results but invokes 2 trig functions
	real quality = 1.0 - fabs(dot(normalize(b - a), normalize(d - a)));
	return quality > del.min_quad_quality;
}

bool is_triangle_facet(Delaunay& d, vertex_handle va, vertex_handle vb, vertex_handle vc) {
	return is_face_corner(d, vb, va, vc);

	vec3 a = d.vertices[va.id].position;
	vec3 b = d.vertices[vb.id].position;
	vec3 c = d.vertices[vc.id].position;

	real ab = length(b - a);
	real ac = length(c - a);
	real bc = length(c - b);

	real quality = 2.0 / (3.0 * sqrt(3)) * length(cross(b - a, c - a)) * (ab + ac + bc) / (ab * ac * bc);

	return quality > d.min_quad_quality;
}

bool is_hex_corner(Delaunay& d, vertex_handle va, vertex_handle vb, vertex_handle vd, vertex_handle ve) {
	/*vec3 a = graph.vertices[va.id].position;
	vec3 b = graph.vertices[vb.id].position;
	vec3 d = graph.vertices[vd.id].position;
	vec3 e = graph.vertices[ve.id].position;

	real ab = length(b - a);
	real ad = length(d - a);
	real ae = length(e - a);

	real quality = dot(cross(b - a, d - a), e - a) / (ab*ad*ae);
	//quality > min_quality;*/
	return is_face_corner(d, va, vb, vd) && is_face_corner(d, va, vb, ve) && is_face_corner(d, va, vd, ve);  
}

//check if face is in tetrahedrilization
bool is_quad_face(Delaunay& del, vertex_handle va, vertex_handle vb, vertex_handle vc, vertex_handle vd) {
	vec3 a = del.vertices[va.id].position;
	vec3 b = del.vertices[vb.id].position;
	vec3 c = del.vertices[vc.id].position;
	vec3 d = del.vertices[vd.id].position;
	
	vec3 t1[3] = { a,b,c };
	vec3 t2[3] = { a,c,d };
	
	return is_face_corner(del, va, vd, vb) && is_face_corner(del, vb, va, vc) && is_face_corner(del, vc, vb, vd) && is_face_corner(del, vd, vc, va);
}

//Binary search is actually slower for small n
void binary_search(vertex_handle v, vertex_handle sorted[8]) {
	//binary search
	int begin = 0;
	int end = 8;
	bool match = 0;
	while (begin != end) {
		uint idx = (begin + end) / 2;
		if (v.id > sorted[idx].id) begin = idx + 1;
		else if (v.id < sorted[idx].id) end = idx;
		else {
			match = true;
			break;
		}
	}
}

void compute_tets(Delaunay& d, VertGraph& graph, vertex_handle verts[8], tet_handle initial, uint v) {
	//(i) have its four vertices be vertices of the cell
	//(ii) have one facet on the boundary and a volume of the same sign than the cell
	//(iii) be adjacent, through a facet that is not on the theoretical cell boundary, to a tetrahedron that respect (i) or (ii).

	/*
	TriangleFaceSet faces[12];
	for (uint i = 0; i < 6; i++) {
		vertex_handle v[4];
		for (uint j = 0; j < 4; j++) {
			v[j] = verts[hexahedron_shape[i][j]];
		}

		faces[i*2 + 0] = TriangleFaceSet(v[0], v[1], v[3]);
		faces[i*2 + 1] = TriangleFaceSet(v[1], v[2], v[3]);
	}

	uint face_opposite_to_vertex[4] = { 2,1,3,0 };
	uint face = face_opposite_to_vertex[v];

	array<10, tet_handle> stack;
	stack.append(initial + face);
	dealloc_tet(d, initial);

	while (stack.length > 0) {
		tet_handle tet = stack.pop();

		for (uint i = 0; i < 4; i++) {
			tet_handle neigh = d.faces[tet + i] & NEIGH;
			if (is_deallocated(d, neigh)) continue;

			TriangleFaceSet face;
			for (uint j = 0; j < 3; j++) face.verts[j] = d.indices[tet + tetra_shape[i][j]];
			sort3_vertices(face.verts);

			bool is_on_boundary = contains(faces, faces + 12, face);
			if (!is_on_boundary) {
				dealloc_tet(d, neigh);
				stack.append(tet);
			}
		}
	}
	*/

	CFDCell cell{CFDCell::HEXAHEDRON};
	for (uint i = 0; i < 8; i++) {
		cell.vertices[i] = verts[i];
	}
	/*compute_normals(d.vertices, cell);

	float quality1 = -dot(cell.faces[0].normal, cell.faces[5].normal);
	float quality2 = -dot(cell.faces[1].normal, cell.faces[3].normal);
	float quality3 = -dot(cell.faces[2].normal, cell.faces[4].normal);
	float quality = (quality1 + quality2 + quality3) / 3;
	if (quality < min_quality) return;*/

	//todo cell connectivity
	d.volume.cells.append(cell);
	//initial is case (i)
	//find inner face
}

void draw_debug_line(Delaunay& d, vertex_handle v0, vertex_handle v1, uint r, uint g, uint b) {
	//draw_line(d.debug, d.vertices[v0.id].position, d.vertices[v1.id].position, vec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0));
}

void debug_suspend_execution(Delaunay& d) {
	//suspend_execution(d.debug);
}

/*
		uint v = tet_vert & NEIGH_FACE;
		tet_handle tet = tet_vert & NEIGH;
		if (is_deallocated(del, tet)) continue;

		vertex_handle a_neighbors[4];
		for (uint i = 0; i < 3; i++) a_neighbors[i] = del.indices[tet + (v + 1 + i) % 4];
		sort3_vertices(a_neighbors);

		vertex_handle a = del.indices[tet_vert];
		vertex_handle b = del[0];
		vertex_handle d = a_neighbors[1];
		vertex_handle e = a_neighbors[2];
*/

//As described by the paper https://arxiv.org/pdf/1705.02451.pdf
void find_hex(Delaunay& del, VertGraph& graph) {
	for (uint i = 0; i < del.vertices.length; i++) {
		vertex_handle a = { (int)i };
		auto a_neighbors = graph.neighbors(a);

		for (uint i = 0; i < a_neighbors.length; i++) {
			vertex_handle b = a_neighbors[i];
			if (b.id <= a.id) continue;

			for (uint j = 0; j < a_neighbors.length; j++) {
				vertex_handle d = a_neighbors[j];
				if (d.id <= b.id) continue;
				if (!is_face_corner(del, a, b, d)) continue;

				for (uint k = 0; k < a_neighbors.length; k++) {
					vertex_handle e = a_neighbors[k];
					if (e.id <= b.id || e.id == d.id) continue;
					if (!is_hex_corner(del, a, b, d, e)) continue;

					draw_debug_line(del, a, b, 255,0,0);
					draw_debug_line(del, a, d, 0,255,0);
					draw_debug_line(del, a, e, 0,0,255);
					debug_suspend_execution(del);

					for (vertex_handle c : graph.neighbors(b, d)) {
						if (c.id <= a.id || c.id == e.id) continue;
						if (!is_quad_face(del, a, b, c, d)) continue;

						for (vertex_handle f : graph.neighbors(b, e)) {
							if (f.id <= a.id || f.id == b.id || f.id == d.id || f.id == e.id || f.id == c.id) continue;
							if (!is_quad_face(del, a, b, f, e)) continue;
							if (!is_hex_corner(del, b, a, c, f)) continue;

							draw_debug_line(del, b, c, 0,0,255);
							draw_debug_line(del, b, f, 0,255,0);
							draw_debug_line(del, f, e, 0, 0, 255);
							debug_suspend_execution(del);

							for (vertex_handle h : graph.neighbors(d, e)) {
								if (h.id <= a.id || h.id == b.id || h.id == c.id || h.id == f.id) continue;
								if (!is_quad_face(del, a, d, h, e)) continue;
								if (!is_hex_corner(del, d, c, a, h)) continue;
								if (!is_hex_corner(del, e, f, h, a)) continue;

								draw_debug_line(del, d, c, 255, 0, 0);
								draw_debug_line(del, d, h, 0, 255, 0);
								draw_debug_line(del, e, h, 0, 0, 255);
								debug_suspend_execution(del);

								for (vertex_handle g : graph.neighbors(c, f, h)) {
									if (g.id <= a.id || g.id == b.id || g.id == d.id || g.id == e.id) continue;
									
									if (!is_hex_corner(del, g, f, h, c)) continue;
									if (!is_hex_corner(del, c, b, d, g)) continue;
									if (!is_hex_corner(del, h, e, d, g)) continue;
									if (!is_hex_corner(del, f, e, g, b)) continue;

									draw_debug_line(del, c, g, 255, 0, 0);
									draw_debug_line(del, f, g, 0, 255, 0);
									draw_debug_line(del, h, g, 0, 0, 255);
									debug_suspend_execution(del);

									//if (!is_quad_face(graph, d, c, g, h)) continue;
									//if (!is_quad_face(graph, e, f, g, h)) continue;
									//if (!is_quad_face(graph, b, c, g, f)) continue;

									vertex_handle verts[8] = { a,b,c,d,e,f,g,h };
									compute_tets(del, graph, verts, 0, 0);
								}
							}
						}
					}
				}
			}
		}
	}
}

bool complete(Delaunay& d) {
	uint cell_offset = d.volume.cells.length;

	//super_cell_base
	for (tet_handle i = 4; i < 4 * d.tet_count; i += 4) {
		if (is_deallocated(d, i)) continue; //Assume continous
		CFDCell cell{ CFDCell::TETRAHEDRON };
		for (uint j = 0; j < 4; j++) {
			cell.vertices[j] = d.indices[i + j];
			if (d.faces[i + j]) cell.faces[j].neighbor = { (int)((d.faces[i + j] / 4) + cell_offset) };
			else cell.faces[j].neighbor = {};
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

	brio_vertices(d.volume, aabb, verts);
	start_with_non_coplanar(d.volume, verts);

	/*vec3 last = d.vertices[verts[0].id].position;
	for (uint i = 1; i < verts.length; i++) {
		vec3 current = d.vertices[verts[i].id].position;
		draw_line(d.debug, last, current, vec4(0, 0, 0, 1));
		last = current;
	}
	suspend_execution(d.debug);*/

	for (uint i = 0; i < verts.length; i++) {
		if (!add_vertex(d, verts[i], min_dist)) {
			draw_debug_boxes(d.volume);
			remove_super(d);
			complete(d);
			printf("Failed at %i out of %i\n", i + 1, verts.length);
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

bool add_point(Delaunay& d, vec3 pos, real min_dist) {
	d.vertices.append({ pos });
	return add_vertex(d, { (int)(d.vertices.length - 1) }, min_dist);
}

void enqueue_vertex(Delaunay& d, vec3 pos) {
	d.vertices.append({ pos });
	d.insertion_order.append({ (int)d.vertices.length - 1 });
}


bool extrude_verts(Delaunay& d, float height, float min_dist) {
	printf("Extruding verts\n");
	d.active_tet = INT_MAX;
	d.active_faces.length = 0;
	d.insertion_order.length = 0;

	uint n = d.vertices.length;

	for (uint i = d.active_vert_offset; i < n; i++) {
		VertexInfo& info = d.active_verts[i - d.active_vert_offset];
		if (info.count == 0) continue;

		vec3 p = d.vertices[i].position;
		vec3 n = info.normal / info.count;
		float len = height; // (1 + 0.5 * info.curvature)* height; // * info.curvature;

		vec3 pos = p + n * len;
		if (!d.aabb.inside(1.1 * pos)) continue;

		enqueue_vertex(d, pos);
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
	info.curvature += dot(c - p, info.normal);
}

bool generate_layer(Delaunay& d, float height) {
	//Clear

	uint n = d.vertices.length - d.active_vert_offset;

	for (uint i = 0; i < n; i++) {
		d.active_verts[i] = {};
	}

	printf("Linear search %i -> %i\n", d.active_tet / 4, d.tet_count);
	for (tet_handle tet = d.active_tet; tet < 4 * d.tet_count; tet += 4) {
		bool eligible = true;

		uint count = 0;

		for (uint i = 0; i < 4; i++) {
			vertex_handle v = d.indices[tet + i];
			if (is_active_vert(d, v)) count++;
			if (!is_or_was_active_vert(d, v)) {
				eligible = false;
				break;
			}
		}

		if (count != 3) eligible = false;
		if (!eligible) continue;

		for (uint i = 0; i < 4; i++) {
			tet_handle face = tet + i;

			bool active_face = true;
			for (uint j = 0; j < 3; j++) {
				vertex_handle v = d.indices[tet + tetra_shape[i][j]];
				if (!is_active_vert(d, v)) active_face = false;
			}

			if (active_face) d.active_faces.append(face);
		}
	}

	for (tet_handle active_face : d.active_faces) {
		//printf("Active face %i\n", active_face);
		tet_handle face = active_face & NEIGH_FACE;
		tet_handle tet = active_face & NEIGH;

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
		tet_handle face = active_face & NEIGH_FACE;
		tet_handle tet = active_face & NEIGH;

		for (uint i = 0; i < 3; i++) {
			uint index = tetra_shape[face][i];
			compute_curvature(d, d.indices[tet + index], d.indices[tet + (index + 1) % 3]);
		}
	}

	return extrude_verts(d, height, 0.25);
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
		d.insertion_order.append({ i });
	}

	start_with_non_coplanar(d.volume, d.insertion_order);
	if (!add_vertices(d, d.insertion_order, height)) return false;

	d.boundary_vert_offset = d.vertices.length;

	//return true;
	//complete(d);
	//return false;

	return extrude_verts(d, height, 0.25);
}

struct RefineTet {
	tet_handle tet;
	float skew;
};

float avg_edge(Delaunay& d, tet_handle tet) {
	float avg_edge = 0;
	uint count = 0;

	for (uint a = 0; a < 4; a++) {
		for (uint b = a + 1; b < 4; b++) {
			vec3 pos1 = d.vertices[d.indices[tet + a].id].position;
			vec3 pos2 = d.vertices[d.indices[tet + b].id].position;
			avg_edge = length(pos1 - pos2);
			count++;
		}
	}
	assert(count == 6);
	return avg_edge / count;
}

bool refine(Delaunay& d, float size) {
	uint num_it = 1;
	for (uint it = 0; it < num_it; it++) {
		uint n = 4 * d.tet_count;

		//LinearRegion region(get_temporary_allocator());

		for (tet_handle i = 4; i < n; i += 4) {
			if (is_deallocated(d, i)) continue;
			vec3 positions[4];
			bool super = false;

			for (uint j = 0; j < 4; j++) {
				vertex_handle v = d.indices[i + j];
				//|| v.id < d.active_vert_offset
				if (is_super_vert(d, v) || v.id < d.active_vert_offset) super = true;
				//if (!d.faces[i+j]) super = true;
				positions[j] = d.vertices[v.id].position;
			}

			if (super) continue;

			float shortest_edge = FLT_MAX;
			float longest_edge = 0;
			for (uint a = 0; a < 4; a++) {
				for (uint b = a + 1; b < 4; b++) {
					float edge = sq_distance(positions[a], positions[b]);
					shortest_edge = fminf(shortest_edge, edge);
					longest_edge = fmaxf(longest_edge, edge);
				}
			}
			shortest_edge = sqrtf(shortest_edge);
			longest_edge = sqrtf(longest_edge);

			bool bad = longest_edge / shortest_edge > 5/3 || shortest_edge > 4 / 3 * size;

			//circum.radius / longest_edge > 0.8
			//2*circum.radius / longest_edge > 0.8
			//circum.radius / longest_edge > 2
			//tetrahedron_skewness(positions) > 0.3
			if (bad) {
				vec3 avg = (positions[0] + positions[1] + positions[2] + positions[3]) / 4;
				Circumsphere circum = circumsphere(positions);

				if (sq_distance(circum.center, avg) > longest_edge * longest_edge) continue;
				vec3 pos = circum.center;

				if (!d.aabb.inside(pos)) continue; // pos = avg;

				vertex_handle v = {(int)d.vertices.length};
				d.vertices.append({ pos });

				d.last = i;
				bool inside_boundary = find_enclosing_tet(d, pos, 0, true);
				if (!inside_boundary) continue;

				//draw_line(d.debug, pos, pos + vec3(0, 0.1, 0), vec4(0, 1, 0, 1));

				if (!find_cavity_faces(d, i, v)) {
					draw_line(d.debug, pos, pos + vec3(0, 1.0, 0), vec4(1, 0, 0, 1));
					suspend_execution(d.debug);

					printf("Failed at %i out of %i, on it %i out of %i", i / 4 + 1, d.tet_count, it + 1, num_it);
					return false;
				}
				
				if (!retriangulate_cavity(d, d.cavity, v)) {
					printf("Failed at %i out of %i, on it %i out of %i", i / 4 + 1, d.tet_count, it + 1, num_it);
					return false;
				}

				/*
				d.last = i;
				if (!add_point(d, pos, 0)) {
					printf("Failed at %i out of %i, on it %i out of %i", i / 4 + 1, d.tet_count, it + 1, num_it);
					return false;
				}
				*/
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
	
	VertGraph graph;
	build_vertex_graph(d, graph);

	for (tet_handle i = 4; i < 4 * d.tet_capacity; i += 4) {
		//if (is_deallocated(d, i)) continue;

		for (uint j = 0; j < 4; j++) {
			bool is_boundary = d.faces[i + j];
			if (is_boundary) {
				for (uint k = 0; k < 3; k++) {
					vertex_handle v = d.indices[i + tetra_shape[j][k]];
					graph.id_to_neighbors[v.id].count = 0;
				}
			}
		}
	}
	
	vec4* avg = TEMPORARY_ZEROED_ARRAY(vec4, d.vertices.length);

	uint num_it = 5;
	for (uint i = 0; i < num_it; i++) {
		uint n = 4 * d.tet_capacity;

		for (int i = 0; i < d.vertices.length; i++) {
			slice<vertex_handle> neighbors = graph.neighbors({ i });
			if (neighbors.length == 0) continue;

			vec3 p0 = d.vertices[i].position;

			vec3 avg = {};
			float total_weight = 0.0f;

			for (vertex_handle v : neighbors) {
				vec3 p1 = d.vertices[v.id].position;
				float weight = 1 / sq(p0 - p1);
				avg += weight * p1;
				total_weight += weight;
			}
			avg /= total_weight;

			d.vertices[i].position = avg;
		}
	}

	return true;
}

void generate_grid(Delaunay& d, float r, vec3 min, vec3 size) {
	d.insertion_order.length = 0;
	d.grid_vert_begin = d.vertices.length;
	for (float n = 0; n < 2.0; n++) {
		for (real a = 0; a < size.x + r; a += r) { //top & bottom
			for (real b = 0; b < size.z + r; b += r) {
				enqueue_vertex(d, min + vec3(a, n * size.y, b));
			}
		}
		for (real a = r; a < size.x; a += r) { //front & back
			for (real b = r; b < size.y; b += r) {
				enqueue_vertex(d, min + vec3(a, b, n * size.z));
			}
		}
		for (real a = r; a < size.y; a += r) { //left & right
			for (real b = 0; b < size.z + r; b += r) {
				enqueue_vertex(d, min + vec3(n * size.x, a, b));
			}
		}
	}
	d.grid_vert_end = d.vertices.length;

	if (!add_vertices(d, d.insertion_order)) return;
}

TetMesh get_tri_mesh(Delaunay& d) {
	//compact first
	tet_handle count = d.tet_capacity;
	for (; count > 0; count--) {
		if (d.subdets[(count - 1) * 4 + 3] != -1) break; //find first non-empty tet
	}

	for (tet_handle i = 4 * (count - 1); i > 0; i -= 4) {
		if (d.subdets[i + 3] == -1) {
			tet_handle mov = count--;
			memcpy_t(d.subdets + i, d.subdets + mov, 4);
			memcpy_t(d.faces + i, d.faces + mov, 4);
			memcpy_t(d.indices + i, d.indices + mov, 4);

			for (uint j = 0; j < 4; j++) d.faces[d.faces[i + j]] = i + j; //update connectivity
		}
	}

	TetMesh result;
	result.faces = d.faces;
	result.indices = d.indices;
	result.tet_count = count;

	return result;
}

TetMesh generate_uniform_tri_mesh(Delaunay& d, CFDSurface& surface, float size) {
	d.insertion_order.length = 0;
	for (int i = 0; i < d.vertices.length; i++) {
		d.insertion_order.append({ i });
	}

	add_vertices(d, d.insertion_order, 0.5 * size);
	refine(d, size);
	smooth(d);
	return get_tri_mesh(d);
}

void generate_n_layers(Delaunay& d, CFDSurface& surface, uint n, float initial, float g, float resolution, uint layers, float min_quad_quality) {
	if (!generate_contour(d, surface, initial)) return;
	printf("Generated contour\n");

	float h = initial;
	for (uint i = 1; i < n; i++) {
		h *= g;
		if (!generate_layer(d, h)) return;
		printf("Generated layer %i out of %i\n", i + 1, n);
		if (d.active_tet == INT_MAX) {
			printf("========= Advancing front completed at %i =========\n", i);
			break;
		}
	}

	AABB& aabb = d.aabb;
	vec3 size = aabb.size();
	float r = 10.0f;

	vector<vertex_handle> boundary_verts;
	vector<Boundary> boundary;
	//build_grid(d, boundary_verts, boundary, resolution, layers);
	//add_vertices(d, boundary_verts);

	generate_grid(d, 10.0f, aabb.min, size);

	printf("Generated grid!\n");

	remove_super(d);
	if (!refine(d, resolution)) {
		complete(d);
		return;
	}
	smooth(d);

	if (min_quad_quality < 1.0) {
		VertGraph vert_graph = {};
		d.min_quad_quality = min_quad_quality;
		build_vertex_graph(d, vert_graph);
		find_hex(d, vert_graph);
	}
	else {
		complete(d);
	}
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
			bowyer.last = { i };
			bowyer.add_vertex(v);
		}
	}
}
#endif

#if 0
void insphere_test() {
	CFDVolume mesh;

	mesh.vertices.append({ vec3(-1,0,0) });
	mesh.vertices.append({ vec3(1,0,0) });
	mesh.vertices.append({ vec3(0,0,1) });
	mesh.vertices.append({ vec3(0,1.0,0.5) });

	CFDCell cell{ CFDCell::TETRAHEDRON };
	for (int i = 0; i < 4; i++) {
		cell.vertices[i] = { i };
	}
	mesh.cells.append(cell);

	vec4 subdets[4];
	compute_subdet(mesh, { subdets,4 }, { 0 });

	mesh.vertices.append({ vec3(0,0.5,0.5) });
	assert(insphere(mesh, { subdets,4 }, { 0 }, { 4 }) > 0);

	mesh.vertices.append({ vec3(0,0.0,0.0) });
	assert(insphere(mesh, { subdets,4 }, { 0 }, { 5 }) > 0);

	mesh.vertices.append({ vec3(-1.1,0,0) });
	assert(insphere(mesh, { subdets,4 }, { 0 }, { 6 }) < 0);
}
#endif
