#pragma once

#include "core/core.h"
#include "core/container/string_buffer.h"
#include "ecs/system.h"

#define APPLICATION_API extern "C" _declspec(dllexport)

struct Modules;
using InitFunction      = void* (*)(void*, Modules&);
using UpdateFunction    = void  (*)(void*, Modules&);
using RenderFunction    = void  (*)(void*, Modules&);
using DeinitFunction    = void  (*)(void*, Modules&);
using ReloadFunction    = void  (*)(void*, Modules&);
using IsRunningFunction = bool  (*)(void*, Modules&);
using RegisterComponents = void(*)(World& world);

class Application {
	string_buffer path;
	void* application_state;
	void* dll_handle;
	u64 time_modified;
	
	InitFunction init_func;
	UpdateFunction update_func;
	RenderFunction render_func;
	IsRunningFunction is_running_func;
	DeinitFunction deinit_func;
	ReloadFunction reload_func;
	RegisterComponents register_components_func;

	void load_functions();

public:
	struct Modules& engine;

	ENGINE_API Application(Modules& engine, string_view);
	ENGINE_API ~Application();

	void ENGINE_API reload();

	void ENGINE_API init(void* = NULL);
	void ENGINE_API update();
	void ENGINE_API render();
	bool ENGINE_API is_running();
	
	void ENGINE_API run();
};