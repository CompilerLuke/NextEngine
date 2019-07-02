#pragma once

#include <glm/vec4.hpp>

struct Device {
	unsigned int width;
	unsigned int height;
	unsigned int multisampling;
	glm::vec4 clear_colour;

	void bind();
};