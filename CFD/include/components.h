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
	glm::vec3 size = glm::vec3(100);
	int contour_layers = 1;
	float contour_initial_thickness = 0.01;
	float contour_thickness_expontent = 1.1;
	
	int tetrahedron_layers = 1;
};