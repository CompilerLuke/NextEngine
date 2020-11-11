#include "components/flyover.h"
#include "engine/input.h"
#include "components/transform.h"
#include "core/io/logger.h"
#include "ecs/system.h"
#include "ecs/ecs.h"

float get_speed(Flyover& self, UpdateCtx& ctx, float height) {
	auto height_multiplier = 1.0f + height / 10.0f; 
	
	if (ctx.input.key_down(Key::Left_Shift))
		return self.movement_speed * 2.0f * height_multiplier * ctx.delta_time;

	return (float)(self.movement_speed * height_multiplier * ctx.delta_time);
}

void update_flyover(World& world, UpdateCtx& ctx) {
	for (auto [e,trans,self]: world.filter<Transform, Flyover>(ctx.layermask)) {
		auto& facing_rotation = trans.rotation;
		auto forward = glm::normalize(facing_rotation * glm::vec3(0, 0, -1));
		auto right = glm::normalize(facing_rotation * glm::vec3(1, 0, 0));

		float vertical_axis = ctx.input.get_vertical_axis();
		float horizontal_axis = ctx.input.get_horizontal_axis();

		float speed = get_speed(self, ctx, trans.position.y);

		trans.position += forward * speed * vertical_axis;
		trans.position += right * speed * horizontal_axis;

		auto last_mouse_offset = ctx.input.mouse_offset * self.mouse_sensitivity;

		if (self.past_movement_speed.length == NUM_PAST_MOVEMENT_SPEEDS) {
			self.past_movement_speed.shift(1);
		}
		self.past_movement_speed.append(last_mouse_offset);

		glm::vec2 mouse_offset = last_mouse_offset;
		/*for (glm::vec2 past_speed : self.past_movement_speed) {
			mouse_offset += past_speed;
		}*/
		
		if (ctx.input.mouse_button_down(MouseButton::Right)) {
			ctx.input.capture_mouse(true);
			//mouse_offset = mouse_offset * (1.0f / self.past_movement_speed.length);
		}
		else {
			ctx.input.capture_mouse(false);
            self.past_movement_speed.clear();
			mouse_offset = glm::vec2(0);
		}

        self.yaw -= mouse_offset.x;
		self.pitch += mouse_offset.y;

		if (self.pitch > 89) 
			self.pitch = 89;
		if (self.pitch < -89) 
			self.pitch = -89;
		if (self.yaw > 360) 
			self.yaw -= 360;
		if (self.yaw < -360) 
			self.yaw += 360;
		
		auto orientation = glm::quat(glm::vec3(glm::radians(self.pitch), glm::radians(self.yaw), 0));
		trans.rotation = orientation;
	}
}
