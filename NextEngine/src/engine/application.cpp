#include "engine/application.h"
#include "engine/engine.h"
#include "core/profiler.h"
#include "engine/input.h"
#include "graphics/rhi/window.h"

#ifdef NE_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void destroy_DLL(void* dll) {
	FreeLibrary((HINSTANCE)dll);
}

void* load_DLL(string_view path) {
	char dest[100];
	int insert_at = path.length - 4;
	memcpy(dest, path.data, insert_at);
	memcpy(dest + insert_at, "copy.dll", 9);

	bool result = CopyFileA(path.c_str(), dest, false);
	if (!result) throw "Could not copy DLL!";

	HINSTANCE dll = LoadLibraryA(dest);
	if (!dll) throw "Could not load DLL!";
	return (void*)dll;
}

void* get_Func(void* dll, const char* name) {
	return GetProcAddress((HINSTANCE)dll, name);
}
#endif

#ifdef __APPLE__
#include <dlfcn.h>
#include <sys/stat.h>
#define _stat stat

void destroy_DLL(void* dll) {
    dlclose(dll);
}

void* load_DLL(string_view path) {
    void* dll = dlopen(path.c_str(), RTLD_NOW);
    if (!dll) throw "Could not load DLL!";
    return (void*)dll;
}

void* get_Func(void* dll, const char* name) {
    return dlsym(dll, name);
}

#endif

Application::Application(Modules& engine, string_view path) : engine(engine), path(path) {}

void Application::init(void* args) {
	load_functions();
    if (register_components_func) register_components_func(*engine.world);
	application_state = init_func(args, engine);
}

u64 timestamp_of(const char* path) {
	struct _stat info;
	_stat(path, &info);

	return info.st_mtime;
}

void Application::load_functions() {
	time_modified = timestamp_of(path.c_str());
	dll_handle = load_DLL(path);

	init_func = (InitFunction)get_Func(dll_handle, "init");
	update_func = (UpdateFunction)get_Func(dll_handle, "update");
    extract_render_func = (ExtractRenderFunction)get_Func(dll_handle, "extract_render_data");
	render_func = (RenderFunction)get_Func(dll_handle, "render");
	is_running_func = (IsRunningFunction)get_Func(dll_handle, "is_running");
	deinit_func = (DeinitFunction)get_Func(dll_handle, "deinit");
	reload_func = (ReloadFunction)get_Func(dll_handle, "reload");
    unload_func = (UnloadFunction)get_Func(dll_handle, "unload");
	register_components_func = (RegisterComponents)get_Func(dll_handle, "register_components");
    

	if (!init_func || !deinit_func || !update_func || !render_func) {
		throw "Missing application functions";
	}
}


void Application::reload() {
    if (unload_func) unload_func(application_state, engine);
    void* previous_dll_handle = dll_handle;
    destroy_DLL(previous_dll_handle);
	load_functions();
    if (register_components_func) register_components_func(*engine.world);
	if (reload_func) reload_func(application_state, engine); //RELOAD may want to use static data from the previous dll, i.e for diffing

}

void Application::update() {
	update_func(application_state, engine);
}

void Application::extract_render_data(FrameData& data) {
    extract_render_func(application_state, engine, data);
}

void Application::render(GPUSubmission& submission) {
	render_func(application_state, engine, submission);
}

bool Application::is_running() {
	return is_running_func(application_state, engine);
}

void Application::reload_if_modified() {
    u64 current_time_stamp = timestamp_of(path.c_str());
    if (current_time_stamp > time_modified) reload();
}

void Application::run() {
	while (is_running()) {
        if (engine.window && !engine.window->is_visible()) {
            sleep(1);
        }
        
		engine.begin_frame();
        
        reload_if_modified();

		{
			Profile profile("Update");
			update();
		}
		
		{
			Profile profile("Render");
			render(*(struct GPUSubmission*)nullptr); //todo implement render callbacks
		}

		engine.end_frame();
	}
}

Application::~Application() {
	deinit_func(application_state, engine);
	destroy_DLL(dll_handle);
}
