#pragma once

#include "core/core.h"
#include <glm/vec2.hpp>
#include "core/container/array.h"

constexpr int NUM_PAST_MOVEMENT_SPEEDS = 3;

COMP
struct Flyover {
	float mouse_sensitivity = 0.005f;
	float movement_speed = 6;
	float yaw = 0;
	float pitch = 0;

	array<3, glm::vec2> past_movement_speed;
};

ENGINE_API void update_flyover(struct World&, struct UpdateCtx&);
