#pragma once

#include "ecs/id.h"
#include "ecs/system.h"
#include "core/reflection.h"

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

	REFLECT()
};

struct Arrow {
	enum State { Fired, AttachedToBow } state = AttachedToBow;
	float duration = 0.0f;

	REFLECT()
};

struct BowSystem : System {
	void update(World& world, UpdateCtx& params);
};