#pragma once

#include "engine/handle.h"
#include <glm/vec3.hpp>

COMP
struct CFDMesh {
	bool concave = true;
	model_handle model;
	glm::vec4 color;
};

COMP 
struct CFDDomain {
	glm::vec3 size = glm::vec3(6,2,4);
	int contour_layers = 3;
	float contour_initial_thickness = 0.01;
	float contour_thickness_expontent = 1.1;
	
	int tetrahedron_layers = 1;
    
    float grid_resolution = 0.15;
    int grid_layers = 0;
    
    glm::vec3 center;
    glm::vec3 plane;
};

