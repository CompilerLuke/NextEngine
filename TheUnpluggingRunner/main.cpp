
#define PROFILING

#include <engine/application.h>
#include <engine/engine.h>
#include <graphics/rhi/window.h>
#include <core/io/vfs.h>
#include <core/io/logger.h>
#include <core/container/string_buffer.h>

#include <core/profiler.h>
#include <stdio.h>

#include "../TheUnpluggingGame/playerInput.h"
#include "../TheUnpluggingGame/fpsController.h"
#include "../TheUnpluggingGame/bowWeapon.h"
#include "../NextEngineEditor/include/editor.h"
#include <ecs/ecs.h>

int main() {
	const char* level = "C:\\Users\\User\\Desktop\\TopCCompiler\\TopCompiler\\Fernix\\assets\\level2\\";
	const char* app_name = "The Unplugging";

	Modules modules(app_name, level);

#ifdef _DEBUG
	const char* game_dll_path = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Debug\\TheUnpluggingGame.dll";
	const char* engine_dll_path = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Debug\\NextEngineEditor.dll";
#else
	const char* game_dll_path = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Release\\TheUnpluggingGame.dll";
	const char* engine_dll_path = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Release\\NextEngineEditor.dll";
#endif

#ifdef BUILD_STANDALONE
	Application game(game_dll_path);
	game.init();
	game.run();
#else
	Application editor(modules, engine_dll_path);
	editor.init((void*)game_dll_path);

	editor.run();

#endif

	return 0;
}