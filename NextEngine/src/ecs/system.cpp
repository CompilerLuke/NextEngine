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

DEFINE_COMPONENT_ID(Entity, 0)
DEFINE_COMPONENT_ID(Transform, 1)
DEFINE_COMPONENT_ID(LocalTransform, 2)
DEFINE_COMPONENT_ID(Materials, 3)
DEFINE_COMPONENT_ID(ModelRenderer, 4)
DEFINE_COMPONENT_ID(Camera, 5)
DEFINE_COMPONENT_ID(Flyover, 6)
DEFINE_COMPONENT_ID(DirLight, 7)
DEFINE_COMPONENT_ID(Skybox, 8)
DEFINE_COMPONENT_ID(CapsuleCollider, 9)
DEFINE_COMPONENT_ID(SphereCollider, 10)
DEFINE_COMPONENT_ID(BoxCollider, 11)
DEFINE_COMPONENT_ID(PlaneCollider, 12)
DEFINE_COMPONENT_ID(RigidBody, 13)
DEFINE_COMPONENT_ID(StaticTransform, 14)

void register_default_systems_and_components(World& world) {
	world.add(new Store<Entity>(100));
	world.add(new Store<Transform>(100));
	world.add(new Store<LocalTransform>(10));
	world.add(new Store<StaticTransform>(100));
	world.add(new Store<Materials>(100));
	world.add(new Store<ModelRenderer>(100));
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

