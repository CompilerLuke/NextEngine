#include "stdafx.h"
#include "components/flyover.h"
#include "core/input.h"
#include "components/transform.h"
#include "logger/logger.h"

REFLECT_STRUCT_BEGIN(Flyover)
REFLECT_STRUCT_MEMBER(mouse_sensitivity)
REFLECT_STRUCT_MEMBER(movement_speed)
REFLECT_STRUCT_END()

float get_speed(Flyover& self, UpdateParams& params, float height) {
	auto height_multiplier = 1.0f + height / 10.0f; 
	
	if (params.input.key_down(GLFW_KEY_LEFT_SHIFT))
		return self.movement_speed * 2.0f * height_multiplier * params.delta_time;

	return (float)(self.movement_speed * height_multiplier * params.delta_time);
}

void FlyOverSystem::update(World& world, UpdateParams& params) {
	auto debugging = world.get<Flyover>();

	for (ID id : world.filter <Flyover, Transform>(params.layermask)) {
		auto trans = world.by_id<Transform>(id);
		auto self = world.by_id<Flyover>(id);

		auto& facing_rotation = trans->rotation;
		auto forward = glm::normalize(facing_rotation * glm::vec3(0, 0, -1));
		auto right = glm::normalize(facing_rotation * glm::vec3(1, 0, 0));

		float vertical_axis = params.input.get_vertical_axis();
		float horizontal_axis = params.input.get_horizontal_axis();

		float speed = get_speed(*self, params, trans->position.y
		
		);
		trans->position += forward * speed * vertical_axis;
		trans->position += right * speed * horizontal_axis;

		auto last_mouse_offset = params.input.mouse_offset * self->mouse_sensitivity;

		if (self->past_movement_speed_length < NUM_PAST_MOVEMENT_SPEEDS) {
			self->past_movement_speed[self->past_movement_speed_length] = last_mouse_offset;
			self->past_movement_speed_length++;
		}
		else {
			glm::vec2 copy_past_movement_speed[NUM_PAST_MOVEMENT_SPEEDS];
			for (int i = 0; i < NUM_PAST_MOVEMENT_SPEEDS; i++)
				copy_past_movement_speed[i] = self->past_movement_speed[i];

			for (int i = 0; i < NUM_PAST_MOVEMENT_SPEEDS - 1; i++) {
				self->past_movement_speed[i] = copy_past_movement_speed[i + 1];
			}
		
			self->past_movement_speed[NUM_PAST_MOVEMENT_SPEEDS - 1] = last_mouse_offset;
		}

		glm::vec2 mouse_offset;
		for (int i = 0; i < NUM_PAST_MOVEMENT_SPEEDS; i++) {
			mouse_offset += self->past_movement_speed[i];
		}
		
		if (params.input.mouse_button_down(Right)) {
			params.input.capture_mouse(true);
			mouse_offset = mouse_offset * (1.0f / self->past_movement_speed_length);
		}
		else {
			params.input.capture_mouse(false);
			mouse_offset = glm::vec2(0);
		}

		

		self->yaw = -mouse_offset.x + self->yaw;
		self->pitch = mouse_offset.y + self->pitch;

		if (self->pitch > 89) 
			self->pitch = 89;
		if (self->pitch < -89) 
			self->pitch = -89;
		if (self->yaw > 360) 
			self->yaw -= 360;
		if (self->yaw < -360) 
			self->yaw += 360;
		
		auto orientation = glm::quat(glm::vec3(glm::radians(self->pitch), glm::radians(self->yaw), 0));
		trans->rotation = orientation;
	}
}