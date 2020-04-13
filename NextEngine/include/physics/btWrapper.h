#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>


struct BulletWrapper;
class btCollisionShape;
class btRigidBody;

struct RigidBodySettings {
	glm::vec3 origin;
	float mass;
	btCollisionShape* shape;
	unsigned int id;
	float sweep_radius;
	bool lock_rotation;
	glm::vec3 velocity;
};

struct BulletWrapperTransform {
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 velocity;
};

btRigidBody* make_RigidBody(BulletWrapper* self, RigidBodySettings* settings);
void free_RigidBody(BulletWrapper* self, btRigidBody* rb);

unsigned int id_of_RigidBody(btRigidBody* body);
void transform_of_RigidBody(btRigidBody* body, BulletWrapperTransform* transform);
void set_transform_of_RigidBody(btRigidBody* body, BulletWrapperTransform* wrapper_transform);

BulletWrapper* make_BulletWrapper();
void free_BulletWrapper(BulletWrapper*);
void step_BulletWrapper(BulletWrapper*, double);

btCollisionShape* make_Heightfield(int 	heightStickWidth,
	int 	heightStickLength,
	const float* 	heightfieldData,
	float 	heightScale,
	float 	minHeight,
	float 	maxHeight
);

btCollisionShape* make_BoxShape(glm::vec3 position);
btCollisionShape* make_SphereShape(float r);
btCollisionShape* make_CapsuleShape(float r, float height);
btCollisionShape* make_PlaneShape(glm::vec3 normal);

void free_collision_shape(btCollisionShape* shape);
