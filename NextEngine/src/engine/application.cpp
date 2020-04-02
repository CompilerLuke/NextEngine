#include "stdafx.h"
#include "engine/application.h"
#include "engine/engine.h"
#include "core/profiler.h"
#include "core/io/input.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void to_wide_char(const char* orig, wchar_t* wcstring, int buffer_length) {
	unsigned int newsize = strlen(orig) + 1;
	assert(newsize <= buffer_length);
	size_t convertedChars;
	mbstowcs_s(&convertedChars, wcstring, newsize, orig, _TRUNCATE);
}

void destroy_DLL(void* dll) {
	FreeLibrary((HINSTANCE)dll);
}

void* load_DLL(string_view path) {
	wchar_t path_w[100];
	to_wide_char(path.c_str(), path_w, 100);

	wchar_t path_w_copy[100];
	int insert_at = path.length - 4;
	to_wide_char(path.c_str(), path_w_copy, 100);
	to_wide_char("copy.dll", path_w_copy + insert_at, 100 - insert_at);

	bool result = CopyFile(path_w, path_w_copy, false);
	if (!result) throw "Could not copy DLL!";

	HINSTANCE dll = LoadLibrary(path_w_copy);
	if (!dll) throw "Could not load DLL!";
	return (void*)dll;
}

void* get_Func(void* dll, const char* name) {
	return GetProcAddress((HINSTANCE)dll, name);
}

Application::Application(Engine& engine, string_view path) : engine(engine), path(path) {}

void Application::init(void* args) {
	load_functions();
	application_state = init_func(args, engine);
}

void Application::load_functions() {
	dll_handle = load_DLL(path);

	init_func = (InitFunction)get_Func(dll_handle, "init");
	update_func = (UpdateFunction)get_Func(dll_handle, "update");
	render_func = (RenderFunction)get_Func(dll_handle, "render");
	is_running_func = (IsRunningFunction)get_Func(dll_handle, "is_running");
	deinit_func = (DeinitFunction)get_Func(dll_handle, "deinint");
	reload_func = (ReloadFunction)get_Func(dll_handle, "reload");
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
		if (engine.input.key_pressed('R')) {
			reload();
		}

		engine.begin_frame();

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