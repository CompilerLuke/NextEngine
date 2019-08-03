#include "stdafx.h"
#include "playerInput.h"
#include <ecs/ecs.h>
#include <core/input.h>

REFLECT_STRUCT_BEGIN(PlayerInput)
REFLECT_STRUCT_MEMBER(mouse_sensitivity)
REFLECT_STRUCT_END()

PlayerInput* get_player_input(World& world) {
	auto result = world.filter<PlayerInput>(game_layer);
	return result.length > 0 ? result[0] : NULL;
}

void PlayerInputSystem::update(World& world, UpdateParams& params) {
	for (PlayerInput* self : world.filter<PlayerInput>(params.layermask)) {
		params.input.capture_mouse(true);

		glm::vec2 mouse_offset = params.input.mouse_offset * self->mouse_sensitivity;

		float yaw = -mouse_offset.x + self->yaw;
		float pitch = mouse_offset.y + self->pitch;

		if (pitch > 89) pitch = 89;
		if (pitch < -89) pitch = -89;
		if (yaw > 360) yaw -= 360;
		if (yaw < -360) yaw += 360;

		self->pitch = pitch;
		self->yaw = yaw;

		self->vertical_axis = params.input.get_vertical_axis();
		self->horizonal_axis = params.input.get_horizontal_axis();

		self->shift = params.input.key_down(GLFW_KEY_LEFT_SHIFT);
		self->space = params.input.key_pressed(' ');

		self->holding_mouse_left = params.input.mouse_button_down(MouseButton::Left);
	}
}