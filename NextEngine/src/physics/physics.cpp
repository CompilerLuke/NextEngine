#include "stdafx.h"
#include "physics/physics.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include <algorithm>

REFLECT_STRUCT_BEGIN(CapsuleCollider)
REFLECT_STRUCT_MEMBER(radius)
REFLECT_STRUCT_MEMBER(height)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(SphereCollider)
REFLECT_STRUCT_MEMBER(radius)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(BoxCollider)
REFLECT_STRUCT_MEMBER(scale)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(PlaneCollider)
REFLECT_STRUCT_MEMBER(normal)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(RigidBody)
REFLECT_STRUCT_MEMBER(mass)
REFLECT_STRUCT_MEMBER(velocity)
REFLECT_STRUCT_MEMBER(override_position)
REFLECT_STRUCT_MEMBER(override_rotation)
REFLECT_STRUCT_MEMBER(override_velocity_x)
REFLECT_STRUCT_MEMBER(override_velocity_y)
REFLECT_STRUCT_MEMBER(override_velocity_z)
REFLECT_STRUCT_MEMBER(continous)
REFLECT_STRUCT_END()

PhysicsSystem::PhysicsSystem() 
: bt_wrapper(make_BulletWrapper()) {
}

PhysicsSystem::~PhysicsSystem() {
	free_BulletWrapper(bt_wrapper);
}

void PhysicsSystem::update(World& world, UpdateParams& params) {
	if (params.layermask & game_layer) 
		step_BulletWrapper(bt_wrapper, params.delta_time);

	for (ID id : world.filter<RigidBody, Transform>(params.layermask)) {
		auto rb = world.by_id<RigidBody>(id);
		auto trans = world.by_id<Transform>(id);

		if (!rb->bt_rigid_body) {
			RigidBodySettings settings;
			settings.origin.x = trans->position.x;
			settings.origin.y = trans->position.y;
			settings.origin.z = trans->position.z;

			btCollisionShape* shape;
			if (world.by_id<SphereCollider>(id)) {
				auto collider = world.by_id<SphereCollider>(id);
				shape = make_SphereShape(collider->radius * trans->scale.x);
				if (rb->continous) {
					settings.sweep_radius = collider->radius * trans->scale.x;
				}
			}
			else if (world.by_id<BoxCollider>(id)) {
				auto collider = world.by_id<BoxCollider>(id);
				glm::vec3 size = collider->scale * trans->scale;
				shape = make_BoxShape(size);
				if (rb->continous) {
					settings.sweep_radius = std::max(size.z, std::max(size.x, size.y));
				}
			}
			else if (world.by_id<CapsuleCollider>(id)) {
				auto collider = world.by_id<CapsuleCollider>(id);
				shape = make_CapsuleShape(collider->radius * trans->scale.x, collider->height * trans->scale.y);
				if (rb->continous) {
					settings.sweep_radius = collider->radius * trans->scale.x + collider->height * trans->scale.y;
				}
			}
			else if (world.by_id<PlaneCollider>(id)) {
				auto collider = world.by_id<PlaneCollider>(id);
				shape = make_PlaneShape(collider->normal);
			}
			
			settings.id = id;
			settings.mass = rb->mass;
			settings.lock_rotation = rb->override_rotation;

			rb->bt_rigid_body = make_RigidBody(bt_wrapper, &settings);
		}

		if (rb->mass == 0) continue;

		BulletWrapperTransform trans_of_rb;

		transform_of_RigidBody(rb->bt_rigid_body, &trans_of_rb);

		if (!rb->override_position) 
			trans->position = trans_of_rb.position;
		else 
			trans_of_rb.position = trans->position;
		
		if (!rb->override_velocity_x)
			rb->velocity.x = trans_of_rb.velocity.x;
		else
			trans_of_rb.velocity.x = rb->velocity.x;

		if (!rb->override_position)
			rb->velocity.z = trans_of_rb.velocity.z;
		else
			trans_of_rb.velocity.z = rb->velocity.z;

		set_transform_of_RigidBody(rb->bt_rigid_body, &trans_of_rb);
	}
}


