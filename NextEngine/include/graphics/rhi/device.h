#pragma once

#include <glm/vec4.hpp>

//THIS FILE WILL BECOME INCREASINGLY IMPORTANT WITH VULKAN

struct Device {
	uint width;
	uint height;
	uint multisampling;

	glm::vec4 clear_colour;

	void bind();
};