#pragma once

#include "core/core.h"
#include "core/container/string_buffer.h"
#include "ecs/system.h"

#define APPLICATION_API extern "C" _declspec(dllexport)

typedef void* (*InitFunction)(void*,  struct Engine&);
typedef void (*UpdateFunction)(void*, struct Engine&);
typedef void (*RenderFunction)(void*, struct Engine&);
typedef void (*DeinitFunction)(void*, struct Engine&);
typedef bool (*IsRunningFunction)(void*, struct Engine&);
typedef void(*ReloadFunction)(void*, struct Engine&);

class Application {
	string_buffer path;
	struct Engine& engine;
	void* application_state;
	void* dll_handle;
	
	InitFunction init_func;
	UpdateFunction update_func;
	RenderFunction render_func;
	IsRunningFunction is_running_func;
	DeinitFunction deinit_func;
	ReloadFunction reload_func;

	void load_functions();

public:
	ENGINE_API Application(Engine& engine, string_view);
	ENGINE_API ~Application();

	void ENGINE_API reload();

	void ENGINE_API init(void* = NULL);
	void ENGINE_API update();
	void ENGINE_API render();
	bool ENGINE_API is_running();
	
	void ENGINE_API run();
};