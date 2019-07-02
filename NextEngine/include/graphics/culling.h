#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <limits>

struct AABB {
	glm::vec3 min = glm::vec3(-FLT_MAX);
	glm::vec3 max = glm::vec3(FLT_MAX);

	void update(const glm::vec3&);
	AABB apply(const glm::mat4&);
	void update_aabb(AABB&);
};