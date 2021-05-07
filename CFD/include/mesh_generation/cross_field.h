#pragma once

#include "cfd_core.h"
#include "core/math/vec3.h"
#include "core/math/vec4.h"
#include "core/container/slice.h"
#include "core/container/vector.h"

struct SurfaceTriMesh;

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
    
    inline glm::mat4 rotation_mat() const {
        glm::mat4 result(
            glm::vec4(glm::vec3(tangent),0.0f),
            glm::vec4(glm::vec3(bitangent),0.0f),
            glm::vec4(glm::vec3(normal),0.0f),
            glm::vec4(0,0,0,1)
        );
        return result;
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
	vector<Cross> edge_flux_boundary;
    vector<uint> edge_flux_stencil[2];
	vector<Cross> theta_cell_center[2];
	vector<float> distance_cell_center[2];
	bool current;

public:
	SurfaceCrossField(SurfaceTriMesh&, CFDDebugRenderer& debug, slice<edge_handle> feature_edges);

	void propagate();

	//vector<FeatureEdge> get_feature_edges();
    
    Cross at_edge(edge_handle edge);
	Cross at_tri(tri_handle tet, vec3 location);
    Cross at_tri(tri_handle tet);
};

void draw_cross(CFDDebugRenderer&, vec3 position, Cross cross, vec4 color);
