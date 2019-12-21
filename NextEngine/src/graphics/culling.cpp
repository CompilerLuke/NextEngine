#include "stdafx.h"
#include "graphics/culling.h"
#include <glm/vec4.hpp>
#include <glm/glm.hpp>
#include "graphics/renderer.h"

void aabb_to_verts(AABB* self, glm::vec4* verts) {
	verts[0] = glm::vec4(self->max.x, self->max.y, self->max.z, 1);
	verts[1] = glm::vec4(self->min.x, self->max.y, self->max.z, 1);
	verts[2] = glm::vec4(self->max.x, self->min.y, self->max.z, 1);
	verts[3] = glm::vec4(self->min.x, self->min.y, self->max.z, 1);

	verts[4] = glm::vec4(self->max.x, self->max.y, self->min.z, 1);
	verts[5] = glm::vec4(self->min.x, self->max.y, self->min.z, 1);
	verts[6] = glm::vec4(self->max.x, self->min.y, self->min.z, 1);
	verts[7] = glm::vec4(self->min.x, self->min.y, self->min.z, 1);
}

AABB AABB::apply(const glm::mat4& matrix) {
	AABB new_aabb;
	
	glm::vec4 verts[8];
	aabb_to_verts(this, verts);

	for (int i = 0; i < 8; i++) {
		glm::vec4 v = matrix * verts[i];
		new_aabb.update(v);
	}
	return new_aabb;
}

void AABB::update_aabb(AABB& other) {
	this->max = glm::max(this->max, other.max);
	this->min = glm::min(this->min, other.min);
}


void AABB::update(const glm::vec3& v) {
	this->max = glm::max(this->max, v);
	this->min = glm::min(this->min, v);
}

void extract_planes(RenderParams& params, glm::vec4 planes[6]) {
	glm::mat4 mat = params.projection * params.view;
	
	for (int i = 0; i < 4; i++) planes[0][i] = mat[i][3] + mat[i][0];
	for (int i = 0; i < 4; i++) planes[1][i] = mat[i][3] - mat[i][0];
	for (int i = 0; i < 4; i++) planes[2][i] = mat[i][3] + mat[i][1];
	for (int i = 0; i < 4; i++) planes[3][i] = mat[i][3] - mat[i][1];
	for (int i = 0; i < 4; i++) planes[4][i] = mat[i][3] + mat[i][2];
	for (int i = 0; i < 4; i++) planes[5][i] = mat[i][3] - mat[i][2];
}

#include "logger/logger.h"

bool cull(glm::vec4 planes[6], const AABB& aabb) {
	bool cull = false;

	for (int planeID = 0; planeID < 6; planeID++) {
		glm::vec3 planeNormal(planes[planeID]);
		float planeConstant(planes[planeID].w);

		glm::vec3 axisVert;

		if (planes[planeID].x < 0) axisVert.x = aabb.min.x;
		else axisVert.x = aabb.max.x;

		if (planes[planeID].y < 0) axisVert.y = aabb.min.y;
		else axisVert.y = aabb.max.y;

		if (planes[planeID].z < 0) axisVert.z = aabb.min.z;
		else axisVert.z = aabb.max.z;

		if (glm::dot(planeNormal, axisVert) + planeConstant < 0.0f) {
			cull = true;
			break;
		}
	}

	return cull;
}




