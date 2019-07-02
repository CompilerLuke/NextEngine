#include "stdafx.h"
#include "ecs/ecs.h"
#include "ecs/system.h"
#include "components/transform.h"
#include "components/camera.h"
#include "core/input.h"
#include "components/flyover.h"
#include "graphics/shader.h"
#include "model/model.h"
#include "graphics/texture.h"
#include "components/lights.h"
#include "graphics/ibl.h"
#include "physics/physics.h"

void register_default_systems_and_components(World& world) {
	world.add(new Store<Entity>(100));
	world.add(new Store<Transform>(10));
	world.add(new Store<LocalTransform>(10));
	world.add(new Store<Materials>(10));
	world.add(new Store<ModelRenderer>(10));
	world.add(new Store<Camera>(3));
	world.add(new Store<Flyover>(1));
	world.add(new Store<DirLight>(2));
	world.add(new Store<Skybox>(1));

	world.add(new Store<CapsuleCollider>(10));
	world.add(new Store<SphereCollider>(10));
	world.add(new Store<BoxCollider>(10));
	world.add(new Store<PlaneCollider>(10));
	world.add(new Store<RigidBody>(10));

	world.add(new CameraSystem());
	world.add(new PhysicsSystem());
	world.add(new FlyOverSystem());
	world.add(new ModelRendererSystem());
	world.add(new LocalTransformSystem());
}

