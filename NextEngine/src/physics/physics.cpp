#include "components/terrain.h"

#include "physics/physics.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include <glm/glm.hpp>

#include "physics/btWrapper.h"
#include <iostream>
#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include "graphics/rhi/buffer.h"
#include "graphics/assets/model.h"

#include "core/io/logger.h"

struct BulletWrapper {
	btDefaultCollisionConfiguration* collisionConfiguration;
	btCollisionDispatcher* dispatcher;
	btBroadphaseInterface* overlappingPairCache;
	btSequentialImpulseConstraintSolver* solver;
	btDiscreteDynamicsWorld* dynamicsWorld;
};

BulletWrapper* make_BulletWrapper() {
	BulletWrapper* self = new BulletWrapper;
	self->overlappingPairCache = new btDbvtBroadphase();

	self->collisionConfiguration = new btDefaultCollisionConfiguration();
	self->dispatcher = new btCollisionDispatcher(self->collisionConfiguration);

	self->solver = new btSequentialImpulseConstraintSolver();
	self->dynamicsWorld = new btDiscreteDynamicsWorld(self->dispatcher, self->overlappingPairCache, self->solver, self->collisionConfiguration);
	self->dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));

	return self;
}

btCollisionShape* make_BoxShape(glm::vec3 position) {
	btCollisionShape* shape = new btBoxShape(btVector3(position.x, position.y, position.z));
	return shape;
}

btCollisionShape* make_SphereShape(float r) {
	btCollisionShape* shape = new btSphereShape(btScalar(r));
	return shape;
}

btCollisionShape* make_CapsuleShape(float r, float height) {
	btCapsuleShape* shape = new btCapsuleShape(r, height);
	return shape;
}

btCollisionShape* make_Heightfield(int 	heightStickWidth,
	int 	heightStickLength,
	const float* 	heightfieldData,
	float 	heightScale,
	float 	minHeight,
	float 	maxHeight
) {	
	return new btHeightfieldTerrainShape(heightStickWidth, heightStickLength, heightfieldData,
		heightScale, minHeight, maxHeight, 1, PHY_FLOAT, false);
}

#include <BulletCollision/CollisionShapes/btShapeHull.h>

btCollisionShape* make_ConvexHull(glm::vec3* data, int num_verts) {
	btConvexHullShape convexHullShape((btScalar*)data, num_verts, sizeof(glm::vec3));
	//create a hull approximation
	convexHullShape.setMargin(0.1f);  // this is to compensate for a bug in bullet
	btShapeHull* hull = new btShapeHull(&convexHullShape);
	hull->buildHull(0);    // note: parameter is ignored by buildHull
	
	
	btConvexHullShape* pConvexHullShape = new btConvexHullShape((btScalar*)data, num_verts, sizeof(glm::vec3));
	
	return pConvexHullShape;
}


btCollisionShape* make_PlaneShape(glm::vec3 normal) {
	return new btStaticPlaneShape(btVector3(normal.x, normal.y, normal.z), 0);
}

btRigidBody* make_RigidBody(BulletWrapper* self, RigidBodySettings* settings) {
	btTransform groundTransform;
	groundTransform.setIdentity();
	groundTransform.setOrigin(btVector3(settings->origin.x, settings->origin.y, settings->origin.z));

	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool isDynamic = (settings->mass != 0.f);

	btVector3 localInertia;
	if (isDynamic) {
		settings->shape->calculateLocalInertia(settings->mass, localInertia);
	}

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* myMotionState = new btDefaultMotionState(groundTransform); //todo use this to sync objects
	btRigidBody::btRigidBodyConstructionInfo rbInfo(settings->mass, myMotionState, settings->shape, localInertia);
	btRigidBody* body = new btRigidBody(rbInfo);

	if (settings->lock_rotation) {
		body->setAngularFactor(btVector3(0, 0, 0));
	}

	if (settings->sweep_radius > 0) {
		body->setCcdMotionThreshold(1e-7f);
		body->setCcdSweptSphereRadius(settings->sweep_radius);
	}

	btVector3 velocity(settings->velocity.x, settings->velocity.y, settings->velocity.z);
	body->setLinearVelocity(velocity);

	int* id = new int;
	*id = settings->id;
	body->setUserPointer(id);

	//add the body to the dynamics world
	self->dynamicsWorld->addRigidBody(body);

	return body;
}

void free_RigidBody(BulletWrapper* self, btRigidBody* body) {
	if (body && body->getMotionState())
	{
		delete body->getMotionState();
	}
	self->dynamicsWorld->removeCollisionObject(body);
}

unsigned int id_of_RigidBody(btRigidBody* body) {
	return *((unsigned int*)body->getUserPointer());
}

void transform_of_RigidBody(btRigidBody* body, BulletWrapperTransform* wrapper_transform) {
	btTransform trans;
	if (body && body->getMotionState())
	{
		body->getMotionState()->getWorldTransform(trans);
	}
	else
	{
		trans = body->getWorldTransform();
	}

	wrapper_transform->position.x = trans.getOrigin().getX();
	wrapper_transform->position.y = trans.getOrigin().getY();
	wrapper_transform->position.z = trans.getOrigin().getZ();

	wrapper_transform->rotation.w = trans.getRotation().getW();
	wrapper_transform->rotation.x = trans.getRotation().getX();
	wrapper_transform->rotation.y = trans.getRotation().getY();
	wrapper_transform->rotation.z = trans.getRotation().getZ();

	wrapper_transform->velocity.x = body->getLinearVelocity().getX();
	wrapper_transform->velocity.y = body->getLinearVelocity().getY();
	wrapper_transform->velocity.z = body->getLinearVelocity().getZ();
}

void set_transform_of_RigidBody(btRigidBody* body, BulletWrapperTransform* wrapper_transform) {
	btTransform trans;
	if (body && body->getMotionState())
	{
		body->getMotionState()->getWorldTransform(trans);
	}
	else
	{
		trans = body->getWorldTransform();
	}

	body->activate();


	btVector3 position(wrapper_transform->position.x, wrapper_transform->position.y, wrapper_transform->position.z);
	btQuaternion rotation;
	rotation.setX(wrapper_transform->rotation.x);
	rotation.setY(wrapper_transform->rotation.y);
	rotation.setZ(wrapper_transform->rotation.z);

	//std::cout << "Set object to position" << position.x() << "," << position.y() << "," << position.z() << std::endl;

	trans.setOrigin(position);
	trans.setRotation(rotation);

	btVector3 velocity(wrapper_transform->velocity.x, wrapper_transform->velocity.y, wrapper_transform->velocity.z);
	body->setLinearVelocity(velocity);
}

void step_BulletWrapper(BulletWrapper* self, double delta) {
	self->dynamicsWorld->stepSimulation(delta, 10);
}


void free_BulletWrapper(BulletWrapper* self) {
	//remove the rigidbodies from the dynamics world and delete them
	for (int i = self->dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--)
	{
		btCollisionObject* obj = self->dynamicsWorld->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMotionState())
		{
			delete body->getMotionState();
		}
		self->dynamicsWorld->removeCollisionObject(obj);
		delete obj;
	}

	delete self->dynamicsWorld->getDebugDrawer();
	delete self->dynamicsWorld;
	delete self->overlappingPairCache;
	delete self->collisionConfiguration;
	delete self->dispatcher;
	delete self;
}

void free_collision_shape(btCollisionShape* shape) {
	delete shape;
}

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

REFLECT_STRUCT_BEGIN(CharacterController)
REFLECT_STRUCT_MEMBER_TAG(on_ground, reflect::HideInInspectorTag)
REFLECT_STRUCT_MEMBER_TAG(velocity, reflect::HideInInspectorTag)
REFLECT_STRUCT_MEMBER(feet_height)
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
	: bt_wrapper(make_BulletWrapper()) {}

void PhysicsSystem::init(World& world) {
	world.on_free<RigidBody>([this, &world](vector<ID>& freed) {
		for (ID id : freed) {
			RigidBody* rb = world.by_id<RigidBody>(id);
			if (rb->bt_rigid_body)
				free_RigidBody(bt_wrapper, rb->bt_rigid_body);
		}
	});
}

PhysicsSystem::~PhysicsSystem() {
	free_BulletWrapper(bt_wrapper);
}

void PhysicsSystem::update(World& world, UpdateCtx& params) {
	if (params.layermask & GAME_LAYER) 
		step_BulletWrapper(bt_wrapper, params.delta_time);

	for (ID id : world.filter<RigidBody, Transform>(params.layermask)) {
		auto rb = world.by_id<RigidBody>(id);
		auto trans = world.by_id<Transform>(id);

		if (!rb->bt_rigid_body) {
			RigidBodySettings settings;
			settings.origin.x = trans->position.x;
			settings.origin.y = trans->position.y;
			settings.origin.z = trans->position.z;
			settings.velocity.x = rb->velocity.x;
			settings.velocity.y = rb->velocity.y;
			settings.velocity.z = rb->velocity.z;

			btCollisionShape* shape = NULL;
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
					settings.sweep_radius = glm::max(size.z, glm::max(size.x, size.y));
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
			else if (world.by_id<Terrain>(id)) {
				auto collider = world.by_id<Terrain>(id);
				int width = collider->width * 32;
				int height = collider->height * 32;

				float size_per_quad = collider->size_of_block / 32.0f;

				//glm::vec3* data = new glm::vec3[width * height]; //todo fix leak

				/*
				for (int h = 0; h < height; h++) {
					for (int w = 0; w < width; w++) {
						auto pos = trans->position + (glm::vec3(w, 0, h) * size_per_quad);

						float height_at_point = collider->heightmap_points[h * height + w] * collider->max_height;

						data[h * height + w] = pos + glm::vec3(0, height_at_point, 0);
					}
				}
				*/

				//settings.origin += glm::vec3(0.5 * size_per_quad * width, 0, -0.5 * size_per_quad * height);
				settings.origin = glm::vec3(0);

				shape = make_PlaneShape(glm::vec3(0, 1, 0));
				//shape = make_ConvexHull(data, width * height);

				//settings.origin = glm::vec3(0);

				//shape = new btHeightfieldTerrainShape(height, width, data,
				//	100, 0.0f, 100.0, 1, PHY_FLOAT, true);

				//shape->setLocalScaling(btVector3(100.0f, 100.f, 100.0f));

				//shape->setLocalScaling(btVector3(size_per_quad, size_per_quad, size_per_quad));
			}

			settings.id = id;
			settings.mass = rb->mass;
			settings.lock_rotation = rb->override_rotation;
			settings.shape = shape;

			if (shape != NULL) {
				rb->bt_rigid_body = make_RigidBody(bt_wrapper, &settings);
			}
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

		if (!rb->override_velocity_y)
			rb->velocity.y = trans_of_rb.velocity.y;
		else
			trans_of_rb.velocity.y = rb->velocity.y;

		if (!rb->override_velocity_z)
			rb->velocity.z = trans_of_rb.velocity.z;
		else
			trans_of_rb.velocity.z = rb->velocity.z;

		set_transform_of_RigidBody(rb->bt_rigid_body, &trans_of_rb);
	}

	auto terrains = world.filter<Terrain>();
	if (terrains.length == 0) return;

	Terrain* terrain = terrains[0];
	Transform* terrain_trans = world.by_id<Transform>(world.id_of(terrain));

	for (ID id : world.filter<CharacterController, CapsuleCollider, Transform>(params.layermask)) {
		Transform* trans = world.by_id<Transform>(id);
		CharacterController* cc = world.by_id<CharacterController>(id);
		CapsuleCollider* collider = world.by_id<CapsuleCollider>(id);

		trans->position += cc->velocity * (float)params.delta_time;

		float height = sample_terrain_height(terrain, terrain_trans, glm::vec2(trans->position.x, trans->position.z));
		height += 0.5f * collider->height + collider->radius;

		if (trans->position.y < height) trans->position.y = height;
		cc->on_ground = trans->position.y == height;
	}
}