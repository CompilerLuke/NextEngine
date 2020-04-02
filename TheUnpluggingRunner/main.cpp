
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

DEFINE_APP_COMPONENT_ID(PlayerInput, 0);
DEFINE_APP_COMPONENT_ID(FPSController, 1);
DEFINE_APP_COMPONENT_ID(Bow, 2);
DEFINE_APP_COMPONENT_ID(Arrow, 3);

REFLECT_STRUCT_BEGIN(FPSController)
REFLECT_STRUCT_MEMBER(roll_cooldown_time)
REFLECT_STRUCT_MEMBER(movement_speed)
REFLECT_STRUCT_MEMBER(roll_speed)
REFLECT_STRUCT_MEMBER(roll_duration)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(PlayerInput)
REFLECT_STRUCT_MEMBER(mouse_sensitivity)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Bow)
REFLECT_STRUCT_MEMBER(attached)
REFLECT_STRUCT_MEMBER(arrow_speed)
REFLECT_STRUCT_MEMBER(reload_time)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Arrow)
REFLECT_STRUCT_END()

DEFINE_APP_COMPONENT_ID(EntityEditor, 15)

REFLECT_STRUCT_BEGIN(EntityEditor)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

void register_components(World& world) {
	world.add(new Store<PlayerInput>(1));
	world.add(new Store<FPSController>(10));
	world.add(new Store<Bow>(5));
	world.add(new Store<Arrow>(20));
	world.add(new Store<EntityEditor>(100));
}

int main() {
	string_view level = "C:\\Users\\User\\Desktop\\TopCCompiler\\TopCompiler\\Fernix\\assets\\level2\\";


	Engine engine(level);
	register_components(engine.world);

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
	Application editor(engine, engine_dll_path);
	editor.init((void*)game_dll_path);
	editor.run();

#endif

	return 0;
}