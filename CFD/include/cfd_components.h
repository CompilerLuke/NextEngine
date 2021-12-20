#pragma once

#include "engine/handle.h"
#include "mesh/input_mesh.h"
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>

REFL
struct input_model_handle {
	uint id = INVALID_HANDLE;
};

COMP
struct CFDMesh {
	bool concave = true;
	input_model_handle model;
	glm::vec4 color;
};

COMP 
struct CFDDomain {
	glm::vec3 size = glm::vec3(6,2,4);
	int contour_layers = 3;
	float contour_initial_thickness = 0.01;
	float contour_thickness_expontent = 1.1;
	
	int tetrahedron_layers = 1;
    
    float grid_resolution = 2.0;
    int grid_layers = 0;
    
    glm::vec3 center;
    glm::vec3 plane = glm::vec3(0,-1,0);

	float quad_quality = 0.8;

	float feature_angle = 30.0f;
	float min_feature_quality = 0.9f;
};

