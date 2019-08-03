#pragma once
#include <ecs/system.h>
#include <reflection/reflection.h>

struct PlayerInput {
	float yaw = 0;
	float pitch = 0;
	float vertical_axis = 0;
	float horizonal_axis = 0;
	float mouse_sensitivity = 0.01;
	bool shift = false;
	bool space = false;
	bool holding_mouse_left = false;

	REFLECT()
};

PlayerInput* get_player_input(struct World&);

struct PlayerInputSystem : System {
	void update(struct World&, UpdateParams& params);
};