#include "core/core.h"
#include "core/string_buffer.h"

#define APPLICATION_API extern "C" _declspec(dllexport)

typedef void* (*InitFunction)(struct Engine*, void*);
typedef void (*UpdateFunction)(void*, Engine*);
typedef void (*RenderFunction)(void*, Engine*);
typedef void (*DeinitFunction)(void*, Engine*);
typedef bool (*IsRunningFunction)(void*, Engine*);

class Application {
	StringBuffer path;
	struct Engine* engine;
	void* application_state;
	void* dll_handle;
	
	InitFunction init_func;
	UpdateFunction update_func;
	RenderFunction render_func;
	IsRunningFunction is_running_func;
	DeinitFunction deinit_func;

	void load_functions();

public:
	ENGINE_API Application(StringView, struct Engine*);
	ENGINE_API ~Application();

	void ENGINE_API reload();

	void ENGINE_API init(void* = NULL);
	void ENGINE_API update();
	void ENGINE_API render();
	bool ENGINE_API is_running();
	
	void ENGINE_API run();
};