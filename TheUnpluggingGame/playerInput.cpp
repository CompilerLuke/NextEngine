#include "stdafx.h"
#include "playerInput.h"
#include <ecs/ecs.h>
#include <engine/input.h>
#include <GLFW/glfw3.h>

PlayerInput* get_player_input(World& world) {
	auto result = world.first<PlayerInput>();
	return result ? &result->get<1>() : nullptr;
}

void update_player_input(World& world, UpdateCtx& ctx) {
	Input& input = ctx.input;

	for (auto[e, self] : world.filter<PlayerInput>(ctx.layermask)) {
		ctx.input.capture_mouse(true);

		glm::vec2 mouse_offset = ctx.input.mouse_offset * self.mouse_sensitivity;

		float yaw = -mouse_offset.x + self.yaw;
		float pitch = mouse_offset.y + self.pitch;

		if (pitch > 89) pitch = 89;
		if (pitch < -89) pitch = -89;
		if (yaw > 360) yaw -= 360;
		if (yaw < -360) yaw += 360;

		self.pitch = pitch;
		self.yaw = yaw;

		self.vertical_axis = input.get_vertical_axis();
		self.horizonal_axis = input.get_horizontal_axis();

		self.shift = input.key_down(Key::Left_Shift);
		self.space = input.key_pressed(Key::Space);

		self.holding_mouse_left = ctx.input.mouse_button_down(MouseButton::Left);
	}
}

