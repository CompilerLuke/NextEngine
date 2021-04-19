#pragma once

#include "mesh/surface_tet_mesh.h"

struct CFDDebugRenderer;

inline float clamp(float low, float high, float value) {
	if (value > high) return high;
	if (value < low) return low;
	return value;
}

struct Cross {
	vec3 tangent;
	vec3 normal;
	vec3 bitangent;

	inline vec3 best_axis(vec3 basis, float* d) {
		float dot_p[3] = {
			dot(tangent, basis),
			dot(normal, basis),
			dot(bitangent, basis)
		};

		uint idx = fabs(dot_p[0]) > fabs(dot_p[1]) ? (fabs(dot_p[0]) > fabs(dot_p[2]) ? 0 : 2) : (fabs(dot_p[1]) > fabs(dot_p[2]) ? 1 : 2);
		vec3 closest = (&tangent)[idx];
		if (dot_p[idx] < 0) {
			closest = -closest;
			dot_p[idx] = -dot_p[idx];
		}

		*d = dot_p[idx];
		return closest;
	}

	inline float angle_between(vec3 basis, vec3 axis) {
		float dot_p[3] = {
			dot(tangent, basis),
			dot(normal, basis),
			dot(bitangent, basis)
		};

		uint idx = fabs(dot_p[0]) > fabs(dot_p[1]) ? (fabs(dot_p[0]) > fabs(dot_p[2]) ? 0 : 2) : (fabs(dot_p[1]) > fabs(dot_p[2]) ? 1 : 2);
		vec3 closest = (&tangent)[idx];
		if (dot_p[idx] < 0) {
			closest = -closest;
			dot_p[idx] = -dot_p[idx];
		}

		float angle = acos(fminf(1.0, dot_p[idx]));
		if (dot(cross(basis, closest), axis) < 0) { // Or > 0
			angle = -angle;
		}
		return angle;
	}
};

/*
struct FeatureEdge {
	edge_handle edge;
	Cross cross1;
	Cross cross2;
};
*/

class SurfaceCrossField {
	SurfaceTriMesh& mesh;
	CFDDebugRenderer& debug;
	slice<edge_handle> feature_edges;

	vector<vec3> centers;
	vector<bool> edge_flux_stencil;
	vector<Cross> edge_flux_boundary;
	vector<Cross> theta_cell_center[2];
	vector<float> distance_cell_center[2];
	bool current;

public:
	SurfaceCrossField(SurfaceTriMesh&, CFDDebugRenderer& debug, slice<edge_handle> feature_edges);

	void propagate();

	//vector<FeatureEdge> get_feature_edges();
	vec3 interpolate(tri_handle tet, vec3 location);
};