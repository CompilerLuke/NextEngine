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

Engine::Engine(string_view level_path) :
	input(*PERMANENT_ALLOC(Input)),
	time(*PERMANENT_ALLOC(Time)),
	world(*PERMANENT_ALLOC(World)),
	window(*PERMANENT_ALLOC(Window)),
	renderer(*PERMANENT_ALLOC(Renderer)),
{
	level.set(level_path);

	register_default_systems_and_components(world);

	window.full_screen = false;
	window.vSync = false;

	window.init();
	input.init(window);

	RHI::create_buffers();
	renderer.init();
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
	gb::window.swap_buffers();
	profile.end();

	Profiler::end_frame();
}