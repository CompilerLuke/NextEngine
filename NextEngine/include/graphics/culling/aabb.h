#pragma once

#include "engine/core.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>

struct AABB {
	glm::vec3 min = glm::vec3(FLT_MAX);
	glm::vec3 max = glm::vec3(-FLT_MAX);

	inline void to_verts(glm::vec4* verts) {
		verts[0] = glm::vec4(max.x, max.y, max.z, 1);
		verts[1] = glm::vec4(min.x, max.y, max.z, 1);
		verts[2] = glm::vec4(max.x, min.y, max.z, 1);
		verts[3] = glm::vec4(min.x, min.y, max.z, 1);

		verts[4] = glm::vec4(max.x, max.y, min.z, 1);
		verts[5] = glm::vec4(min.x, max.y, min.z, 1);
		verts[6] = glm::vec4(max.x, min.y, min.z, 1);
		verts[7] = glm::vec4(min.x, min.y, min.z, 1);
	}

	inline void update(const glm::vec3& v) {
		this->max = glm::max(this->max, v);
		this->min = glm::min(this->min, v);
	}

	inline AABB apply(const glm::mat4& matrix) {
		AABB new_aabb;

		glm::vec4 verts[8];
		to_verts(verts);

		for (int i = 0; i < 8; i++) {
			glm::vec4 v = matrix * verts[i];
			new_aabb.update(v);
		}
		return new_aabb;
	}

	inline void update_aabb(AABB& other) {
		this->max = glm::max(this->max, other.max);
		this->min = glm::min(this->min, other.min);
	}

	inline glm::vec3 operator[](int i) const { return (&min)[i]; };
	inline glm::vec3 centroid() { return (min + max) / 2.0f; };
	inline glm::vec3 size() { return max - min; };
};

