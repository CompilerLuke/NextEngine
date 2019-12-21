#include "stdafx.h"
#include "engine/application.h"

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

void* load_DLL(StringView path) {
	wchar_t path_w[100];
	to_wide_char(path.c_str(), path_w, 100);
	HINSTANCE dll = LoadLibrary(path_w);

	return (void*)dll;
}

void* get_Func(void* dll, const char* name) {
	return GetProcAddress((HINSTANCE)dll, name);
}

Application::Application(StringView path, struct Engine* engine) : path(path), engine(engine) {}

void Application::init(void* args) {
	load_functions();
	application_state = init_func(engine, args);
}

void Application::load_functions() {
	dll_handle = load_DLL(path);

	init_func = (InitFunction)get_Func(dll_handle, "init");
	update_func = (UpdateFunction)get_Func(dll_handle, "update");
	render_func = (RenderFunction)get_Func(dll_handle, "render");
	is_running_func = (IsRunningFunction)get_Func(dll_handle, "is_running");
	deinit_func = (DeinitFunction)get_Func(dll_handle, "deinint");
}

void Application::reload() {
	destroy_DLL(dll_handle);
	load_functions();
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
		update();
		render();
	}
}

Application::~Application() {
	deinit_func(application_state, engine);
	destroy_DLL(dll_handle);
}