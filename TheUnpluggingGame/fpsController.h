#pragma once

#include <ecs/system.h>

COMP
struct FPSController {
	float movement_speed = 0;
	float roll_speed = 0;
	float roll_duration = 0;
	float roll_cooldown = 0;
	float roll_cooldown_time = 1.0;
	float roll_blend = 0;
};

void update_FPSControllers(struct World&, struct UpdateCtx&);
