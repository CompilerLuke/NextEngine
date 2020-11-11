#include "stdafx.h"
#include "bowWeapon.h"
#include <ecs/ecs.h>
#include "playerInput.h"
#include "physics/physics.h"
#include "components/transform.h"
#include <glm/gtc/matrix_transform.hpp>
#include "core/reflection.h"
#include "core/io/logger.h"
#include "core/serializer.h"
#include "component_ids.h"

#include <ecs/system.h>

ID clone(World& world, ID id) {
	return 0;
	
	//return world.make_ID();
	
	/*
	ID new_id = world.make_ID();

	for (int i = 0; i < world.components_hash_size; i++) {
		ComponentStore* store = world.components[i].get();
		if (store) {
			Component src = store->get_by_id(id);

			if (src.data) {
				Component src = store->get_by_id(id);
				void* dest = store->make_by_id(new_id);

				SerializerBuffer buffer_write;
				buffer_write.write_struct((refl::Struct*)src.type, src.data);

				DeserializerBuffer buffer_read(buffer_write.data, buffer_write.index);
				buffer_read.read_struct((refl::Struct*)src.type, dest);
			}
		}
	}*/

	//return new_id;
}

#include <glm/gtc/quaternion.hpp>

glm::quat quat_look_rotation(glm::vec3 direction, glm::vec3 upDirection) {
	return glm::conjugate(glm::quat_cast(
		glm::lookAt(glm::vec3(0,0,0),
			direction,
			upDirection
		)
	));
	
	/*
	auto forward = lookAt;
	auto up = upDirection;

	//compute rotation axis
	glm::vec3 rotAxis = glm::normalize(glm::cross(front, lookAt));
	//if (glm::normalize(glm::pow(rotAxis, glm::vec3(2))) == 0)
	//	rotAxis = up;

	//find the angle around rotation axis
	float dot = glm::dot(front, lookAt);
	float ang = std::acosf(dot);

	return glm::angleAxis(ang, rotAxis);
	*/

	/*
	forward = glm::normalize(forward);
	up = up - (forward * glm::dot(up, forward));
	up = glm::normalize(up);

	auto vector = forward;
	auto vector2 = glm::cross(up, vector);
	auto vector3 = glm::cross(vector, vector2);
	auto m00 = vector2.x;
	auto m01 = vector2.y;
	auto m02 = vector2.z;
	auto m10 = vector3.x;
	auto m11 = vector3.y;
	auto m12 = vector3.z;
	auto m20 = vector.x;
	auto m21 = vector.y;
	auto m22 = vector.z;

	auto num8 = (m00 + m11) + m22;

	if (num8 > 0.0) {
		auto num = glm::sqrt(num8 + 1.0);
		auto w = num * 0.5;
		num = 0.5 / num;
		return glm::quat(
			(m12 - m21) * num,
			(m20 - m02) * num,
			(m01 - m10) * num,
			w
		);
	}
	else if (m00 >= m11 && m00 >= m22) {
		auto num7 = glm::sqrt(1.0 + m00 - m11 - m22);
		auto num4 = 0.5 / num7;

		return glm::quat(
			0.5 * num7,
			(m01 + m10) * num4,
			(m02 + m20) * num4,
			(m12 - m21) * num4
		);
	}
	else if (m11 > m22) {
		auto num6 = glm::sqrt(1.0 + m11 - m00 - m22);
		auto num3 = 0.5 / num6;
		return glm::quat(
			 (m10 + m01) * num3,
			0.5 * num6,
			(m21 + m12) * num3,
			(m20 - m02) * num3
		);
	}
	else {
		auto num5 = glm::sqrt(1.0 + m22 - m00 - m11);
		auto num2 = 0.5 / num5;
		return glm::quat(
			(m20 + m02) * num2,
			(m21 + m12) * num2,
			0.5 * num5,
			(m01 - m10) * num2
		);
	}
	*/
}

void update_bows(World& world, UpdateCtx& params) {
	PlayerInput* input = get_player_input(world);

	for (auto[e, trans, local, rb, arrow] : world.filter<Transform, LocalTransform, RigidBody, Arrow>(params.layermask)) {
		if (arrow.state == Arrow::Fired) {
			arrow.duration -= params.delta_time;
			if (arrow.duration <= 0) {
				world.free_by_id(e.id);
			}
			else {
				glm::mat4 rot = glm::lookAt(glm::vec3(0, 0, 0), glm::normalize(rb.velocity), glm::vec3(0, 1, 0));

				trans.rotation = quat_look_rotation(-rb.velocity, glm::vec3(0, 1, 0));
			}
		}
	}

	for (auto[e, trans, local, bow] : world.filter<Transform, LocalTransform, Bow>(params.layermask)) {
		auto camera_components = world.get_by_id<LocalTransform, Transform>(local.owner);
		if (!camera_components) continue;

		auto[cam_trans, cam_world_trans] = *camera_components;

		switch (bow.state) {
		case Bow::Idle: {
			if (input->holding_mouse_left) {
				bow.duration = 0.1;
				bow.state = Bow::Charging;
			}
			break;
		}

		case Bow::Charging: {
			local.rotation = glm::angleAxis(glm::radians(-87.0f), glm::vec3(0,0,1));

			if (input->holding_mouse_left) {
				LocalTransform* arrow_trans = world.m_by_id<LocalTransform>(bow.attached);

				if (bow.duration <= 1.5) {
					arrow_trans->position.z += params.delta_time * 0.15;
					bow.duration += params.delta_time;
				} 
			}
			else {
				calc_global_transform(world, bow.attached); 
				bow.state = Bow::Firing;
			}
			break;
		}
		case Bow::Firing: {
			float speed = bow.duration * bow.arrow_speed;

			Arrow* arrow = world.m_by_id<Arrow>(bow.attached);
			if (!arrow) continue;

			LocalTransform* arrow_local = world.m_by_id<LocalTransform>(bow.attached);
			if (!arrow_local) continue;

			if (true) { //arrow_local->position.z <= -0.8) {
				arrow->state = Arrow::Fired;
				arrow->duration = 5.0f;

				ID cloned = clone(world, bow.attached);

				/*
				arrow_local->position.z = arrow_local->position.z - params.delta_time * speed;
				calc_global_transform(world, bow.attached); 
				world.free_by_id<LocalTransform>(bow.attached);

				BoxCollider* box = world.make<BoxCollider>(bow.attached);
				box->scale = glm::vec3(0.1f, 0.1f, 0.5f);
				RigidBody* rb = world.make<RigidBody>(bow.attached);
				rb->mass = 0.1f;
				rb->override_rotation = true;
				rb->velocity = cam_world_trans.rotation * glm::vec3(0, 0, -speed);
			
				bow.attached = cloned;

				bow.state = Bow::Reloading;
				bow.duration = bow.reload_time;

				world.by_id<Entity>(cloned).enabled = false;
				world.by_id<LocalTransform>(cloned)->position.z = -0.4;

				local.rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 0, 1));
				*/
			}
			else {
				arrow_local->position.z -= params.delta_time * speed;
			}

			break;
		}

		case Bow::Reloading: {
			bow.duration -= params.delta_time;
			if (bow.duration <= 0) {
				bow.state = Bow::Idle;

				//world.by_id<Entity>(bow.attached)->enabled = true;
				calc_global_transform(world, bow.attached); 
			}
			break;
		}
		}
	}
}
