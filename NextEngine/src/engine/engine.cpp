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
#include "graphics/assets/assets.h"
#include "core/time.h"

#include "physics/physics.h"
#include "components/transform.h"
#include "graphics/rhi/vulkan/vulkan.h"

Modules::Modules(const char* app_name, const char* level_path) {
	window = new Window();
	input = new Input();
	time = new Time();
	world = new World();	
	local_transforms_system = new LocalTransformSystem();
	physics_system = new PhysicsSystem();

	register_default_systems_and_components(*world);
	physics_system->init(*world);

	window->title = app_name;
	window->full_screen = false;
	window->vSync = true;

	window->init();
	input->init(*window);

	const char* validation_layers[1] = {
		"VK_LAYER_KHRONOS_validation"
	};

	VulkanDesc vk_desc = {};
	vk_desc.api_version = VK_MAKE_VERSION(1, 2, 0);
	vk_desc.app_name = app_name;
	vk_desc.app_version = VK_MAKE_VERSION(0, 0, 0);
	vk_desc.engine_name = "NextEngine";
	vk_desc.engine_version = VK_MAKE_VERSION(0, 0, 0);
	vk_desc.min_log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	vk_desc.validation_layers = validation_layers;
	vk_desc.num_validation_layers = 1;
	vk_desc.device_features.samplerAnisotropy = VK_TRUE;
	vk_desc.device_features.multiDrawIndirect = VK_TRUE;

	rhi = make_RHI(vk_desc, *window);
	
	assets = make_Assets(*rhi, level_path);
	renderer = new Renderer(*rhi, *assets, *window, *world);
	
	init_RHI(rhi, *assets);
}

Modules::~Modules() {
	delete window;
	delete input;
	delete time;
	delete world;
	destroy_RHI(rhi);
	destroy_Assets(assets);
	delete physics_system;
	delete local_transforms_system;
}

void Modules::begin_frame() {
	Profiler::begin_frame();

	Profile profile("Reset Frame");

	world->begin_frame();
	input->clear();
	window->poll_inputs();
	time->tick();
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

void Modules::end_frame() {
	Profile profile("Swap Buffers");
	window->swap_buffers();
	profile.end();

	Profiler::end_frame();
}

