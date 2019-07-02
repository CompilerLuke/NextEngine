#include "stdafx.h"
#include "components/camera.h"
#include "components/transform.h"
#include <glm/gtc/matrix_transform.hpp>

REFLECT_STRUCT_BEGIN(Camera)
REFLECT_STRUCT_MEMBER(near_plane)
REFLECT_STRUCT_MEMBER(far_plane)
REFLECT_STRUCT_MEMBER(fov)
REFLECT_STRUCT_END()

void Camera::update_matrices(World& world, RenderParams& params) {
	auto transform = world.by_id<Transform>(world.id_of(this));

	params.projection = glm::perspective(
		glm::radians(this->fov),
		(float)params.width / (float)params.height,
		this->near_plane,
		this->far_plane
	);

	auto rotate_m = glm::mat4_cast(transform->rotation);
	rotate_m = glm::inverse(rotate_m);

	glm::mat4 translate_m(1.0);
	translate_m = glm::translate(translate_m, -transform->position);

	params.view = rotate_m * translate_m;
	params.cam = this;
}

Camera* get_camera(World& world, Layermask layermask) {
	return world.filter<Camera>(layermask)[0];
}

void CameraSystem::render(World& world, RenderParams& params) {
	auto camera = get_camera(world, params.layermask);
	camera->update_matrices(world, params);
}