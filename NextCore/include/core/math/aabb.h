#pragma once

#include "core/core.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>

#undef max
#undef min

struct AABB {
	glm::vec3 min = glm::vec3(FLT_MAX);
	glm::vec3 max = glm::vec3(-FLT_MAX);

	inline void to_verts(glm::vec3* verts) const {
		verts[0] = glm::vec3(min.x, min.y, min.z);
		verts[1] = glm::vec3(max.x, min.y, min.z);
		verts[2] = glm::vec3(max.x, min.y, max.z);
		verts[3] = glm::vec3(min.x, min.y, max.z);
						   
		verts[4] = glm::vec3(min.x, max.y, min.z);
		verts[5] = glm::vec3(max.x, max.y, min.z);
		verts[6] = glm::vec3(max.x, max.y, max.z);
		verts[7] = glm::vec3(min.x, max.y, max.z);
	}

	inline void update(const glm::vec3& v) {
		this->max = glm::max(this->max, v);
		this->min = glm::min(this->min, v);
	}

	inline AABB apply(const glm::mat4& matrix) {
		AABB new_aabb;

		glm::vec3 verts[8];
		to_verts(verts);

		for (int i = 0; i < 8; i++) {
			glm::vec4 v = matrix * glm::vec4(verts[i], 1.0);
			new_aabb.update(v);
		}
		return new_aabb;
	}

	inline void update_aabb(const AABB& other) {
		this->max = glm::max(this->max, other.max);
		this->min = glm::min(this->min, other.min);
	}

	inline bool operator==(const AABB& other) const {
		return max == other.max && min == other.min;
	}

	inline bool operator!=(const AABB& other) const {
		return !(*this == other);
	}

	inline bool inside(const AABB& other) const { 
		return glm::min(min, other.min) == other.min
			&& glm::max(max, other.max) == other.max;
	}

	inline bool inside(glm::vec3 other) const {
		return glm::min(min, other) == min
			&& glm::max(max, other) == max;
	}

	inline bool intersects(const AABB& other) const {
		return (min.x <= other.max.x && max.x >= other.min.x) &&
			   (min.y <= other.max.y && max.y >= other.min.y) &&
			   (min.z <= other.max.z && max.z >= other.min.z);
	}

	inline glm::vec3 operator[](int i) const { return (&min)[i]; };
	inline glm::vec3 centroid() const { return (min + max) / 2.0f; };
	inline glm::vec3 size() const { return max - min; };
};

