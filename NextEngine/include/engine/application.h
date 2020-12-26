#pragma once

#include "engine/core.h"
#include "core/container/string_buffer.h"
#include "ecs/system.h"

#ifdef NE_PLATFORM_WINDOWS
#define APPLICATION_API extern "C" _declspec(dllexport)
#else
#define APPLICATION_API extern "C"
#endif

struct GPUSubmission;
struct FrameData;

struct Modules;
using InitFunction      = void* (*)(void*, Modules&);
using UpdateFunction    = void  (*)(void*, Modules&);
using ExtractRenderFunction = void (*)(void*, Modules&, FrameData&);
using RenderFunction    = void  (*)(void*, Modules&, GPUSubmission&, FrameData&);
using DeinitFunction    = void  (*)(void*, Modules&);
using UnloadFunction    = void  (*)(void*, Modules&);
using ReloadFunction    = void  (*)(void*, Modules&);
using IsRunningFunction = bool  (*)(void*, Modules&);
using RegisterComponents = void(*)(World& world);

class Application {
	string_buffer path;
	void* dll_handle;
	u64 time_modified;
	
	InitFunction init_func;
	UpdateFunction update_func;
    ExtractRenderFunction extract_render_func;
	RenderFunction render_func;
	IsRunningFunction is_running_func;
	DeinitFunction deinit_func;
	ReloadFunction reload_func;
    UnloadFunction unload_func;
	RegisterComponents register_components_func;

	void load_functions();

public:
	void* application_state;
	struct Modules& engine;

	ENGINE_API Application(Modules& engine, string_view);
	ENGINE_API ~Application();

	void ENGINE_API reload();
    void ENGINE_API reload_if_modified();

	void ENGINE_API init(void* = NULL);
	void ENGINE_API update();
    void ENGINE_API extract_render_data(FrameData&);
	void ENGINE_API render(GPUSubmission&, FrameData&);
	bool ENGINE_API is_running();
	
	void ENGINE_API run();

	ENGINE_API void* get_func(string_view name);
};
