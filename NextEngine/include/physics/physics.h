#pragma once

#include "ecs/id.h"
#include "ecs/system.h"
#include <glm/vec3.hpp>
#include "core/reflection.h"

struct btRigidBody;

COMP struct CapsuleCollider {
	float radius;
	float height;
};

COMP struct SphereCollider {
	float radius;
};

COMP struct BoxCollider {
	glm::vec3 scale;
};

COMP struct PlaneCollider {
	glm::vec3 normal;
};

COMP struct RigidBody {
	float mass = 0;
	glm::vec3 velocity;
	bool override_position = false;
	bool override_rotation = false;

	bool override_velocity_x = false;
	bool override_velocity_y = false;
	bool override_velocity_z = false;

	bool continous = false;

	REFL_FALSE btRigidBody* bt_rigid_body = NULL;
    REFL_FALSE RigidBody() = default;
    REFL_FALSE RigidBody(const RigidBody& other) {
        memcpy(this, &other, sizeof(RigidBody));
        bt_rigid_body = nullptr;
    }
};

COMP struct CharacterController {
	bool on_ground = true;
	glm::vec3 velocity;

	float feet_height = 1;
};

struct PhysicsSystem : System {
	struct BulletWrapper* bt_wrapper;

	PhysicsSystem();
	void init(struct World&);
	~PhysicsSystem();

	void update(struct World&, UpdateCtx&) override;
};
