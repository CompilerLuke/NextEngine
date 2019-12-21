#pragma once

#include "ecs/id.h"
#include "ecs/system.h"
#include <glm/vec3.hpp>
#include "physics/btWrapper.h"
#include "reflection/reflection.h"

struct CapsuleCollider {
	float radius;
	float height;
	REFLECT(ENGINE_API)
};

struct SphereCollider {
	float radius;
	REFLECT(ENGINE_API)
};

struct BoxCollider {
	glm::vec3 scale;
	REFLECT(ENGINE_API)
};

struct PlaneCollider {
	glm::vec3 normal;
	REFLECT(ENGINE_API)
};

struct RigidBody {
	float mass = 0;
	glm::vec3 velocity;
	bool override_position = false;
	bool override_rotation = false;

	bool override_velocity_x = false;
	bool override_velocity_y = false;
	bool override_velocity_z = false;

	bool continous = false;

	struct btRigidBody* bt_rigid_body = NULL;
	REFLECT(ENGINE_API)
};

struct CharacterController {
	bool on_ground = true;
	glm::vec3 velocity;

	float feet_height = 1;

	REFLECT(ENGINE_API)
};

struct PhysicsSystem : System {
	BulletWrapper* bt_wrapper;

	PhysicsSystem(struct World&);
	~PhysicsSystem();

	void update(struct World&, UpdateParams&) override;
};