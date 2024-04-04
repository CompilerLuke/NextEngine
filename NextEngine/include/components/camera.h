#pragma once

#include "engine/core.h"
#include "ecs/id.h"
#include <glm/mat4x4.hpp>
#include "core/math/intersection.h"

COMP
struct Camera {
	float near_plane = 0.1f;
	float far_plane = 10;
	float fov = 60;
};

struct Transform;
struct Camera;
struct World;
struct Viewport;


ENGINE_API glm::mat4 get_view_matrix(Transform& trans);
ENGINE_API glm::mat4 get_proj_matrix(Camera& camera, float asp);
ENGINE_API void update_camera_matrices(Transform& trans, Camera& camera, Viewport& viewport);
ENGINE_API void update_camera_matrices(World& world, EntityQuery layermask, Viewport& viewport);

ENGINE_API ID get_camera(World& world, EntityQuery layermask);

ENGINE_API Ray ray_from_mouse(struct Viewport&, glm::vec2 mouse_position);