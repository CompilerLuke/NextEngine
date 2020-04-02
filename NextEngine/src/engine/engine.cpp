#include "stdafx.h"
#include "engine/engine.h"
#include "graphics/renderer/renderer.h"
#include "core/io/input.h"
#include "physics/physics.h"
#include "core/profiler.h"
#include "core/io/vfs.h"
#include "graphics/rhi/buffer.h"
#include "core/memory/linear_allocator.h"
#include "ecs/ecs.h"
#include "graphics/assets/asset_manager.h"
#include "core/time.h"

#include "physics/physics.h"
#include "components/transform.h"

Engine::Engine(string_view level_path) :
	level(level_path),
	input(*new Input()),
	time(*new Time()),
	world(*new World()),
	window(*new Window()),
	renderer(*new Renderer()),
	asset_manager(*new AssetManager(level)),
	local_transforms_system(*new LocalTransformSystem()),
	physics_system(*new PhysicsSystem())
{
	register_default_systems_and_components(world);

	physics_system.init(world);

	window.full_screen = false;
	window.vSync = false;

	window.init();
	input.init(window);

	RHI::create_buffers();
	renderer.init(asset_manager, window, world);
}

Engine::~Engine() {
	destruct(&input);
	destruct(&time);
	destruct(&world);
	destruct(&window);
	destruct(&renderer);
	destruct(&asset_manager);
}

void Engine::begin_frame() {
	Profiler::begin_frame();

	Profile profile("Reset Frame");

	world.begin_frame();
	input.clear();
	window.poll_inputs();
	time.tick();
	temporary_allocator.clear();

	/*
	world.add(new TransformSystem());
	world.add(new SkyboxSystem(world));
	world.add(new PhysicsSystem(world));
	world.add(new FlyOverSystem()); //todo move into editor
	world.add(new ModelRendererSystem());
	world.add(new GrassSystem());
	world.add(new LocalTransformSystem());
	*/
}

void Engine::end_frame() {
	Profile profile("Swap Buffers");
	window.swap_buffers();
	profile.end();

	Profiler::end_frame();
}

