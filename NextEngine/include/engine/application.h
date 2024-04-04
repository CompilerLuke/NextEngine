#pragma once

#include "engine/core.h"
#include "core/container/string_buffer.h"
#include "ecs/system.h"
#include <memory>

#ifdef NE_PLATFORM_WINDOWS
#define APPLICATION_API extern "C" _declspec(dllexport)
#else
#define APPLICATION_API extern "C"
#endif

struct Modules;

class Application {
public:
    virtual Modules& get_modules() = 0;
    virtual void ENGINE_API reload();
    virtual void ENGINE_API reload_if_modified();
    virtual void ENGINE_API update() = 0;
    virtual void ENGINE_API render() = 0;
    virtual bool ENGINE_API is_running();

    virtual void ENGINE_API run();
    virtual ENGINE_API ~Application();
};

class Dynamic_Application : public Application {
    std::unique_ptr<Application> app;
	Modules& engine;
    string_buffer path;
	void* dll_handle;
	u64 time_modified;

    using InitFunction = Application*(*)(Modules&, void*);
    InitFunction init_func = {};

	void load_functions();

public:
	ENGINE_API Dynamic_Application(Modules& engine, string_view path, void* args);
	ENGINE_API ~Dynamic_Application();

	void ENGINE_API reload() override;
    void ENGINE_API reload_if_modified() override;

    Modules& ENGINE_API get_modules() override;

	void ENGINE_API update() override;
	void ENGINE_API render() override;
	bool ENGINE_API is_running() override;

	ENGINE_API void* get_func(string_view name);
};
