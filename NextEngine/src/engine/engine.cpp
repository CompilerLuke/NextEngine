#include "engine/engine.h"
#include "graphics/renderer/renderer.h"
#include "engine/input.h"
#include "physics/physics.h"
#include "core/profiler.h"
#include "engine/vfs.h"
#include "graphics/rhi/buffer.h"
#include "core/memory/linear_allocator.h"
#include "ecs/ecs.h"
#include "graphics/assets/assets.h"
#include "core/time.h"

#include "physics/physics.h"
#include "components/transform.h"
#include "graphics/rhi/vulkan/vulkan.h"

#include "graphics/rhi/window.h"

#include "core/job_system/job.h"

Modules::Modules(const char* app_name, const char* level_path, const char* engine_asset_path) {
	window = new Window();
	input = new Input();
	time = new Time();
	world = new World(WORLD_SIZE);	
	physics_system = new PhysicsSystem();

	register_default_components(*world);
	physics_system->init(*world);

    window->width = 2048;
    window->height = 2048;
	window->title = app_name;
	window->full_screen = false;
	window->vSync = false;

	window->init();
	input->init(*window);

	const char* validation_layers[1] = {
		"VK_LAYER_KHRONOS_validation"
	};

	VulkanDesc vk_desc = {};
    vk_desc.api_version = VK_API_VERSION_1_2; // VK_MAKE_VERSION(1, 2, 0);
	vk_desc.app_name = app_name;
	vk_desc.app_version = VK_MAKE_VERSION(0, 0, 0);
	vk_desc.engine_name = "NextEngine";
	vk_desc.engine_version = VK_MAKE_VERSION(0, 0, 0);
	vk_desc.min_log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	vk_desc.validation_layers = validation_layers;
	
/*#ifdef NDEBUG
	vk_desc.num_validation_layers = 0;
#else
	vk_desc.num_validation_layers = 1;
#endif*/

    vk_desc.num_validation_layers = 1;
    //vk_desc.num_validation_layers = 0;

	vk_desc.device_features.samplerAnisotropy = VK_TRUE;
	vk_desc.device_features.multiDrawIndirect = VK_TRUE;
	vk_desc.device_features.fillModeNonSolid = VK_TRUE;
    //vk_desc.device_features.wideLines = VK_TRUE;

	make_RHI(vk_desc, *window);
	make_AssetManager(level_path, engine_asset_path);

	RenderSettings settings = {};
    settings.display_resolution_width = 2048;
	settings.display_resolution_height = 2048;
	settings.shadow_resolution = 512;

	renderer = make_Renderer(settings, *world);
}

Modules::~Modules() {
	delete window;
	delete input;
	delete time;
	delete world;
	destroy_Renderer(renderer);
	destroy_RHI();
	destroy_AssetManager();
	delete physics_system;
	delete local_transforms_system;
}

/*
	world.add(new TransformSystem());
	world.add(new SkyboxSystem(world));
	world.add(new PhysicsSystem(world));
	world.add(new FlyOverSystem()); //todo move into editor
	world.add(new ModelRendererSystem());
	world.add(new GrassSystem());
	world.add(new LocalTransformSystem());
	*/


void Modules::begin_frame() {
	Profiler::begin_frame();

	Profile profile("Reset Frame");

	world->begin_frame();
	input->clear();
	window->poll_inputs();
	time->tick();
}

void clear_temporary(void*) {
	get_thread_local_temporary_allocator().clear();
}

void Modules::end_frame() {
	Profile profile("Swap Buffers");
	window->swap_buffers();
	profile.end();

	Profiler::end_frame();

	uint schedule_on[MAX_THREADS];
	JobDesc job_desc[MAX_THREADS];

	uint count = worker_thread_count() - 1;
	for (uint i = 0; i < count; i++) {
		job_desc[i] = JobDesc(clear_temporary, nullptr);
		schedule_on[i] = i + 1;
	}

	schedule_jobs_on({ schedule_on, count}, { job_desc, count }, nullptr);
	clear_temporary(nullptr);
}

