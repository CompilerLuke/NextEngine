#include "ecs/ecs.h"
#include "ecs/system.h"
#include "components/transform.h"
#include "components/camera.h"
#include "core/io/input.h"
#include "components/flyover.h"
#include "graphics/assets/shader.h"
#include "graphics/assets/model.h"
#include "graphics/assets/texture.h"
#include "components/lights.h"
#include "graphics/renderer/ibl.h"
#include "physics/physics.h"
#include "components/terrain.h"
#include "components/grass.h"
#include "graphics/assets/material.h"


void register_default_systems_and_components(World& world) {
	world.add_component<Entity>();
	world.add_component<Transform>();
	world.add_component<LocalTransform>();
	world.add_component<StaticTransform>();
	world.add_component<Materials>();
	world.add_component<ModelRenderer>();
	world.add_component<Camera>();
	world.add_component<Flyover>();
	world.add_component<DirLight>();
	world.add_component<Skybox>();
	world.add_component<Terrain>();
	world.add_component<TerrainControlPoint>();

	world.add_component<CapsuleCollider>();
	world.add_component<SphereCollider>();
	world.add_component<BoxCollider>();
	world.add_component<PlaneCollider>();
	world.add_component<RigidBody>();
	world.add_component<Grass>();
	world.add_component<CharacterController>();

	world.add_component<PointLight>();
	world.add_component<SkyLight>();
}

/*
{
	world.add(new TransformSystem());
	world.add(new SkyboxSystem(world));
	world.add(new PhysicsSystem(world));
	world.add(new FlyOverSystem()); //todo move into editor
	world.add(new ModelRendererSystem());
	world.add(new GrassSystem());
	world.add(new LocalTransformSystem());
}
*/

