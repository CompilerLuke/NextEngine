#include "stdafx.h"
#include "physics/btWrapper.h"
#include <iostream>
#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>

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
		std::cout << "set continous collision" << std::endl;
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
	self->dynamicsWorld->stepSimulation(delta);
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

	delete self->dynamicsWorld;
	delete self->overlappingPairCache;
	delete self->collisionConfiguration;
	delete self->dispatcher;
	delete self;
}

void free_collision_shape(btCollisionShape* shape) {
	delete shape;
}