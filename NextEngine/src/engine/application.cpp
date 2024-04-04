#include "engine/application.h"
#include "engine/engine.h"
#include "core/profiler.h"
#include "engine/input.h"
#include "graphics/rhi/window.h"
#include "core/job_system/thread.h"
#include "graphics/renderer/renderer.h"

void Application::reload() {}
void Application::reload_if_modified() {}
bool Application::is_running() {
    return !get_modules().window->should_close();
}
Application::~Application() noexcept {}

void Application::run() {
    Modules& engine = get_modules();
    static bool present = true;

    while (is_running()) {
        if (!present && engine.window && !engine.window->is_visible()) {
            thread_sleep(16000);
            present = false;
            engine.window->poll_inputs();

            continue;
        }

        engine.begin_frame();

        reload_if_modified();

        {
            Profile profile("Update");
            update();
        }

        if (!engine.headless) {
            Profile profile("Render");

            FrameData frame = {};
            render();
        }

        engine.end_frame();
        present = false;
    }
}

#ifdef NE_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void destroy_DLL(void* dll) {
	FreeLibrary((HINSTANCE)dll);
}

void* load_DLL(string_view path, bool* dll_is_locked = nullptr) {
	char dest[100];
	int insert_at = path.length - 4;
	memcpy(dest, path.data, insert_at);
	memcpy(dest + insert_at, "copy.dll", 9);

	printf("Making a copy of the dll %s\n", path.data);


	bool result = CopyFileA(path.c_str(), dest, false);
	if (!result) {
		printf("Could not copy DLL!\n");
		if (dll_is_locked) *dll_is_locked = true;
		return nullptr;
	}

	if (dll_is_locked) *dll_is_locked = false;

	printf("Loading the dll %s\n", dest);

	HINSTANCE dll = LoadLibraryA(dest);
	
	if (!dll) {
		printf("Error Code: %i\n", GetLastError());
		throw "Could not load DLL!";
	}
	return (void*)dll;
}

void* get_Func(void* dll, const char* name) {
	return GetProcAddress((HINSTANCE)dll, name);
}
#endif

#ifdef NE_PLATFORM_MACOSX
#include <dlfcn.h>
#include <sys/stat.h>
#define _stat stat

void destroy_DLL(void* dll) {
    dlclose(dll);
}

void* load_DLL(string_view path, bool* locked) {
    void* dll = dlopen(path.c_str(), RTLD_NOW);
    if (!dll) throw "Could not load DLL!";
    return (void*)dll;
}

void* get_Func(void* dll, const char* name) {
    return dlsym(dll, name);
}

#endif

Dynamic_Application::Dynamic_Application(Modules& engine, string_view path, void* args) : engine(engine), path
(path) {
    load_functions();
    app = std::unique_ptr<Application>(init_func(engine, args));
}

Modules& Dynamic_Application::get_modules() {
    return engine;
}

u64 timestamp_of(const char* path) {
	struct _stat info;
	_stat(path, &info);
    
	return info.st_mtime; 
}

void* Dynamic_Application::get_func(string_view name) {
	return get_Func(dll_handle, name.c_str());
}

void Dynamic_Application::load_functions() {
	bool locked = false;
	for (uint i = 0; i < 10; i++) {
		dll_handle = load_DLL(path, &locked);
		if (!locked) break;

		thread_sleep(100);
	}

	if (!dll_handle) {
		fprintf(stderr, "Failed to load dll!\n");
		return;
	}

	init_func = (InitFunction)get_Func(dll_handle, "init");


	if (!init_func) {
        if (!init_func) fprintf(stderr, "Missing init function 'APPLICATION_API std::unique_ptr<Application*> init"
                                        "(Modules&, void* args)");
        throw "Missing application functions";
	}

    time_modified = timestamp_of(path.c_str());
}


void Dynamic_Application::reload() {
    // todo: fixuo VTABLEs
    void* previous_dll_handle = dll_handle;
    destroy_DLL(previous_dll_handle);
	load_functions();

    printf("Reloading!\n");
}

void Dynamic_Application::update() {
	app->update();
}

void Dynamic_Application::render() {
	app->render();
}

bool Dynamic_Application::is_running() {
	return app->is_running();
}

void Dynamic_Application::reload_if_modified() {
    u64 current_time_stamp = timestamp_of(path.c_str());
    if (current_time_stamp > time_modified) reload();
}

Dynamic_Application::~Dynamic_Application() {
	app = nullptr;
	destroy_DLL(dll_handle);
}
