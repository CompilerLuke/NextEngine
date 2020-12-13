#pragma once

#include "core/core.h"
#include <glm/vec3.hpp>

COMP
struct CFDMesh {
	bool concave = true;
};

COMP 
struct CFDDomain {
	glm::vec3 size;
	int contour_layers;
	float contour_thickness;
};