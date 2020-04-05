#include "stdafx.h"

#ifdef RENDER_API_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include "components/camera.h"
#include "components/transform.h"
#include <glm/gtc/matrix_transform.hpp>
#include "graphics/renderer/renderer.h"

REFLECT_STRUCT_BEGIN(Camera)
REFLECT_STRUCT_MEMBER(near_plane)
REFLECT_STRUCT_MEMBER(far_plane)
REFLECT_STRUCT_MEMBER(fov)
REFLECT_STRUCT_END()

glm::mat4 get_view_matrix(World& world, ID id) {
	auto camera = world.by_id<Camera>(id);
	auto transform = world.by_id<Transform>(id);

	auto rotate_m = glm::mat4_cast(transform->rotation);
	rotate_m = glm::inverse(rotate_m);

	glm::mat4 translate_m(1.0);
	translate_m = glm::translate(translate_m, -transform->position);

	return rotate_m * translate_m;
}

glm::mat4 get_proj_matrix(World& world, ID id, float asp) {
	auto camera = world.by_id<Camera>(id);
	
	return glm::perspective(
		glm::radians(camera->fov),
		asp,
		camera->near_plane,
		camera->far_plane
	);
}

void update_camera_matrices(World& world, ID id, RenderCtx& ctx) {
	ctx.projection = get_proj_matrix(world, id, (float)ctx.width / ctx.height);
	ctx.view = get_view_matrix(world, id);
	ctx.cam = world.by_id<Camera>(id);
	ctx.view_pos = world.by_id<Transform>(id)->position;
}

ID get_camera(World& world, Layermask layermask) {
	return world.id_of(world.filter<Camera>(layermask)[0]);
}
