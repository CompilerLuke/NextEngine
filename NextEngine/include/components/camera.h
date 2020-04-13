#pragma once

#include "ecs/ecs.h"
#include "core/reflection.h"

struct Camera {
	float near_plane = 0.1f;
	float far_plane = 600;
	float fov = 60;

	REFLECT(ENGINE_API)
};

ENGINE_API glm::mat4 get_view_matrix(World& world, ID id);
ENGINE_API glm::mat4 get_proj_matrix(World& world, ID id, float asp);
ENGINE_API void update_camera_matrices(World& world, ID id, RenderCtx&);

ENGINE_API ID get_camera(World& world, Layermask layermask);