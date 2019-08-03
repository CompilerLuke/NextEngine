#include "stdafx.h"
#include "fpsController.h"
#include <ecs/ecs.h>
#include <components/transform.h>
#include "playerInput.h"
#include <physics/physics.h>

REFLECT_STRUCT_BEGIN(FPSController)
REFLECT_STRUCT_MEMBER(roll_cooldown_time)
REFLECT_STRUCT_MEMBER(movement_speed)
REFLECT_STRUCT_MEMBER(roll_speed)
REFLECT_STRUCT_MEMBER(roll_duration)
REFLECT_STRUCT_END()

void FPSControllerSystem::update(World& world, UpdateParams& params) {
	PlayerInput* player_input = get_player_input(world);
	
	for (ID id : world.filter<FPSController, LocalTransform>(params.layermask)) {
		FPSController* self = world.by_id<FPSController>(id);
		LocalTransform* trans = world.by_id<LocalTransform>(id);

		RigidBody* rb = world.by_id<RigidBody>(trans->owner);
		if (rb == NULL) continue;

		float pitch = player_input->pitch;
		float yaw = player_input->yaw;

		glm::quat facing_rotation = glm::quat(glm::vec3(0, glm::radians(yaw), 0));
		glm::vec3 forward = glm::normalize(facing_rotation * glm::vec3(0, 0, -1));
		glm::vec3 right = glm::normalize(facing_rotation * glm::vec3(1, 0, 0));
		
		self->roll_cooldown -= params.delta_time;
		self->roll_cooldown = glm::max(0.0f, self->roll_cooldown);

		if (player_input->shift && self->roll_cooldown <= 0) {
			self->roll_cooldown = self->roll_cooldown_time;
			self->roll_blend = 1;
		}

		float vel = (self->roll_speed * self->roll_blend) + (self->movement_speed * (1.0 - self->roll_blend));

		self->roll_blend -= params.delta_time / self->roll_duration;
		self->roll_blend = glm::max(0.0f, self->roll_blend);

		glm::vec3 vec = forward * player_input->vertical_axis * vel;
		vec += right * player_input->horizonal_axis * vel;


		rb->velocity.x = vec.x;
		rb->velocity.z = vec.z;

		if (player_input->space) {
			rb->override_velocity_y = true;
			rb->velocity.y = 5;
		}
		else {
			rb->override_velocity_y = false;
		}

		trans->rotation = glm::quat(glm::vec3(glm::radians(pitch), glm::radians(yaw), 0));
	}
}