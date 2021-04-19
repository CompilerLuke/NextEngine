#include "components/camera.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include <glm/gtc/matrix_transform.hpp>
#include "graphics/renderer/renderer.h"

glm::mat4 get_view_matrix(Transform& trans) {
	auto rotate_m = glm::mat4_cast(trans.rotation);
	rotate_m = glm::inverse(rotate_m);

	glm::mat4 translate_m(1.0);
	translate_m = glm::translate(translate_m, -trans.position);

	return rotate_m * translate_m;
}


glm::mat4 get_proj_matrix(Camera& camera, float asp) {	
	return glm::perspective(
		glm::radians(camera.fov),
		asp,
		camera.near_plane,
		camera.far_plane
	);
}

void update_camera_matrices(Transform& trans, Camera& camera, Viewport& viewport) {
	viewport.proj = get_proj_matrix(camera, (float)viewport.width / viewport.height);
	viewport.view = get_view_matrix(trans);
	viewport.cam_pos = trans.position;

	//viewport.cam = world.by_id<Camera>(id);
	//ctx.view_pos = world.by_id<Transform>(id)->position;
}

void update_camera_matrices(World& world, EntityQuery flags, Viewport& viewport) {
	auto[e, trans, camera] = *world.first<Transform, Camera>(flags);
	update_camera_matrices(trans, camera, viewport);
	//viewport.cam = world.by_id<Camera>(id);
	//ctx.view_pos = world.by_id<Transform>(id)->position;
}

ID get_camera(World& world, EntityQuery layermask) {
	auto [e,_] = *world.first<Camera>(layermask);
	return e.id;
}

Ray ray_from_mouse(Viewport& viewport, glm::vec2 mouse_position) {
	glm::mat4 inv_proj_view = glm::inverse(viewport.proj * viewport.view);

	glm::vec2 mouse_position_clip = mouse_position / glm::vec2(viewport.width, viewport.height);
	mouse_position_clip.y = 1.0 - mouse_position_clip.y;
	mouse_position_clip = mouse_position_clip * 2.0f - 1.0f;

	glm::vec4 end = inv_proj_view * glm::vec4(mouse_position_clip, 1, 1.0);

#ifdef GLM_FORCE_DEPTH_ZERO_TO_ONE
	glm::vec4 start = inv_proj_view * glm::vec4(mouse_position_clip, 0.0f, 1.0);
#else 
	glm::vec4 start = inv_proj_view * glm::vec4(mouse_position_clip, -1, 1.0);
#endif

	start /= start.w;
	end /= end.w;

	return Ray(start, glm::normalize(end - start), glm::length(end - start));
}