#pragma once

#include "core/core.h"
#include "graphics/pass/pass.h"
#include "ecs/layermask.h"
#include <glm/mat4x4.hpp>

COMP
struct Camera {
	float near_plane = 0.1f;
	float far_plane = 600;
	float fov = 60;
};

struct Transform;
struct Camera;
struct World;

ENGINE_API glm::mat4 get_view_matrix(Transform& trans);
ENGINE_API glm::mat4 get_proj_matrix(Camera& camera, float asp);
ENGINE_API void update_camera_matrices(Transform& trans, Camera& camera, Viewport& viewport);
ENGINE_API void update_camera_matrices(World& world, ID id, Viewport& viewport);

ENGINE_API ID get_camera(World& world, Layermask layermask);