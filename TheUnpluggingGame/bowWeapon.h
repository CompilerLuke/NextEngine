#pragma once

#include "ecs/id.h"

COMP
struct Bow {
	ID attached;

	enum State {
		Charging,
		Reloading,
		Firing,
		Idle
	} state = Idle;
	
	float duration = 0.0f;

	float arrow_speed = 30.0f;
	float reload_time = 1.0f;
};

COMP
struct Arrow {
	enum State { Fired, AttachedToBow } state = AttachedToBow;
	float duration = 0.0f;
};

void update_bows(struct World& world, struct UpdateCtx& ctx);