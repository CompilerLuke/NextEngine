#pragma once

#include "ecs/ecs.h"
#include <glm/vec2.hpp>

constexpr int NUM_PAST_MOVEMENT_SPEEDS = 3;

struct Flyover {
	float mouse_sensitivity = 0.005f;
	float movement_speed = 6;
	float yaw = 0;
	float pitch = 0;

	glm::vec2 past_movement_speed[NUM_PAST_MOVEMENT_SPEEDS];
	int past_movement_speed_length = 0;

	REFLECT()
};

struct FlyOverSystem : System {
	void update(World&, UpdateParams&);
};