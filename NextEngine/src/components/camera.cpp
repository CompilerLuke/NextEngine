#include "components/camera.h"
#include "components/transform.h"
#include <glm/gtc/matrix_transform.hpp>
#include "graphics/renderer/renderer.h"

REFLECT_STRUCT_BEGIN(Camera)
REFLECT_STRUCT_MEMBER(near_plane)
REFLECT_STRUCT_MEMBER(far_plane)
REFLECT_STRUCT_MEMBER(fov)
REFLECT_STRUCT_END()

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

	//viewport.cam = world.by_id<Camera>(id);
	//ctx.view_pos = world.by_id<Transform>(id)->position;
}

void update_camera_matrices(World& world, ID id, Viewport& viewport) {
	update_camera_matrices(*world.by_id<Transform>(id), *world.by_id<Camera>(id), viewport);
	//viewport.cam = world.by_id<Camera>(id);
	//ctx.view_pos = world.by_id<Transform>(id)->position;
}

ID get_camera(World& world, Layermask layermask) {
	return world.id_of(world.filter<Camera>(layermask)[0]);
}
