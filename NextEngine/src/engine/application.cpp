#include "engine/application.h"
#include "engine/engine.h"
#include "core/profiler.h"
#include "engine/input.h"

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

Application::Application(Modules& engine, string_view path) : engine(engine), path(path) {}

void Application::init(void* args) {
	load_functions();
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
	render_func = (RenderFunction)get_Func(dll_handle, "render");
	is_running_func = (IsRunningFunction)get_Func(dll_handle, "is_running");
	deinit_func = (DeinitFunction)get_Func(dll_handle, "deinit");
	reload_func = (ReloadFunction)get_Func(dll_handle, "reload");
	register_components_func = (RegisterComponents)get_Func(dll_handle, "register_components");

	if (!init_func || !deinit_func || !update_func || !render_func) {
		throw "Missing application functions";
	}
}


void Application::reload() {
	destroy_DLL(dll_handle);
	load_functions();
	if (reload_func) reload_func(application_state, engine);
}

void Application::update() {
	update_func(application_state, engine);
}

void Application::render() {
	render_func(application_state, engine);
}

bool Application::is_running() {
	return is_running_func(application_state, engine);
}

void Application::run() {
	while (is_running()) {
		engine.begin_frame();

		{
			u64 current_time_stamp = timestamp_of(path.c_str());
			if (current_time_stamp > time_modified) reload();
		}

		{
			Profile profile("Update");
			update();
		}
		
		{
			Profile profile("Render");
			render();
		}

		engine.end_frame();
	}
}

Application::~Application() {
	deinit_func(application_state, engine);
	destroy_DLL(dll_handle);
}