#include "components/terrain.h"
#include "physics/physics.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include <glm/glm.hpp>

#include <iostream>
#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include "graphics/rhi/buffer.h"
#include "graphics/assets/model.h"

#include "core/io/logger.h"
#include "ecs/component_ids.h"

struct BulletWrapper {
	btDefaultCollisionConfiguration* collisionConfiguration;
	btCollisionDispatcher* dispatcher;
	btBroadphaseInterface* overlappingPairCache;
	btSequentialImpulseConstraintSolver* solver;
	btDiscreteDynamicsWorld* dynamicsWorld;
};

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

PhysicsSystem::PhysicsSystem()
	: bt_wrapper(make_BulletWrapper()) {}

void PhysicsSystem::init(World& world) {
	//todo implement creation/ destruction callbacks!
	
	/*
	world.on_free<RigidBody>([this, &world](vector<ID>& freed) {
		for (ID id : freed) {
			RigidBody* rb = world.by_id<RigidBody>(id);
			if (rb->bt_rigid_body)
				free_RigidBody(bt_wrapper, rb->bt_rigid_body);
		}
	});
	*/
}

PhysicsSystem::~PhysicsSystem() {
	free_BulletWrapper(bt_wrapper);
}

btVector3 to_bt_vec3(glm::vec3 vec) {
	return { vec.x, vec.y, vec.z };
}

glm::vec3 from_bt_vec3(btVector3 vec) {
	return {vec.getX(), vec.getY(), vec.getZ() };
}

RigidBodySettings default_rb_settings(Entity e, Transform& trans, RigidBody& rb) {
	RigidBodySettings settings;
	settings.id = e.id;
	settings.mass = rb.mass;
	settings.lock_rotation = rb.override_rotation;

	return settings;
}

void PhysicsSystem::update(World& world, UpdateCtx& params) {
	step_BulletWrapper(bt_wrapper, params.delta_time);

	EntityQuery to_be_initialized = params.layermask.with_none<BtRigidBodyPtr>();

	for (auto [e, trans, collider, rb] : world.filter<Transform, SphereCollider, RigidBody>(to_be_initialized)) {
		RigidBodySettings settings = default_rb_settings(e, trans, rb);
		settings.shape = make_SphereShape(collider.radius * trans.scale.x);
		if (rb.continous) settings.sweep_radius = collider.radius * trans.scale.x;
		
		world.add<BtRigidBodyPtr>(e.id)->bt_rigid_body = make_RigidBody(bt_wrapper, &settings);
	}

	for (auto [e, trans, collider, rb] : world.filter<Transform, BoxCollider, RigidBody>(to_be_initialized)) {
		RigidBodySettings settings = default_rb_settings(e, trans, rb);

		glm::vec3 size = collider.scale * trans.scale;
		settings.shape = make_BoxShape(size);
		if (rb.continous) settings.sweep_radius = glm::max(size.z, glm::max(size.x, size.y));

		world.add<BtRigidBodyPtr>(e.id)->bt_rigid_body = make_RigidBody(bt_wrapper, &settings);
	}

	for (auto [e, trans, collider, rb] : world.filter<Transform, CapsuleCollider, RigidBody>(to_be_initialized)) {
		RigidBodySettings settings = default_rb_settings(e, trans, rb);
		settings.shape = make_CapsuleShape(collider.radius * trans.scale.x, collider.height * trans.scale.y);
		if (rb.continous) settings.sweep_radius = collider.radius * trans.scale.x + collider.height * trans.scale.y;

		world.add<BtRigidBodyPtr>(e.id)->bt_rigid_body = make_RigidBody(bt_wrapper, &settings);
	}

	for (auto [e, trans, collider, rb] : world.filter<Transform, PlaneCollider, RigidBody>(to_be_initialized)) {
		RigidBodySettings settings = default_rb_settings(e, trans, rb);
		settings.shape = make_PlaneShape(collider.normal);

		world.add<BtRigidBodyPtr>(e.id)->bt_rigid_body = make_RigidBody(bt_wrapper, &settings);
	}

	for (auto [e, trans, collider, rb] : world.filter<Transform, Terrain, RigidBody>(to_be_initialized)) {
		RigidBodySettings settings = default_rb_settings(e, trans, rb);
		int width = collider.width * 32;
		int height = collider.height * 32;

		float size_per_quad = collider.size_of_block / 32.0f;

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
		settings.shape = make_PlaneShape(glm::vec3(0, 1, 0));
		//shape = make_ConvexHull(data, width * height);

		//settings.origin = glm::vec3(0);

		//shape = new btHeightfieldTerrainShape(height, width, data,
		//	100, 0.0f, 100.0, 1, PHY_FLOAT, true);

		//shape->setLocalScaling(btVector3(100.0f, 100.f, 100.0f));

		//shape->setLocalScaling(btVector3(size_per_quad, size_per_quad, size_per_quad));

		world.add<BtRigidBodyPtr>(e.id)->bt_rigid_body = make_RigidBody(bt_wrapper, &settings);
	}

	for (auto [e, ptr] : world.filter<BtRigidBodyPtr>(params.layermask.with_none<RigidBody>())) {
		free_RigidBody(bt_wrapper, ptr.bt_rigid_body);
	}

	for (auto [e,trans,ptr,rb]: world.filter<Transform, BtRigidBodyPtr, RigidBody>(params.layermask)) {
		if (rb.mass == 0) continue;

		btRigidBody* bt_rigid_body = ptr.bt_rigid_body;
		BulletWrapperTransform trans_of_rb;

		transform_of_RigidBody(bt_rigid_body, &trans_of_rb);

		if (!rb.override_position) trans.position = trans_of_rb.position;
		else trans_of_rb.position = trans.position;
		
		if (!rb.override_velocity_x) rb.velocity.x = trans_of_rb.velocity.x;
		else trans_of_rb.velocity.x = rb.velocity.x;

		if (!rb.override_velocity_y) rb.velocity.y = trans_of_rb.velocity.y;
		else trans_of_rb.velocity.y = rb.velocity.y;

		if (!rb.override_velocity_z) rb.velocity.z = trans_of_rb.velocity.z;
		else trans_of_rb.velocity.z = rb.velocity.z;

		set_transform_of_RigidBody(bt_rigid_body, &trans_of_rb);
	}

	auto terrains = world.first<Terrain, Transform>();
	if (!terrains) return;

	auto [terrain_e, terrain, terrain_trans] = *terrains;

	for (auto [e,trans,cc,collider] : world.filter<Transform, CharacterController, CapsuleCollider>(params.layermask)) {
		trans.position += cc.velocity * (float)params.delta_time;

		float height = sample_terrain_height(terrain, terrain_trans, glm::vec2(trans.position.x, trans.position.z));
		height += 0.5f * collider.height + collider.radius;

		if (trans.position.y < height) trans.position.y = height;
		cc.on_ground = trans.position.y == height;
	}
}
