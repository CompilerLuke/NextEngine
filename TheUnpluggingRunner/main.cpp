
#define PROFILING

#include <engine/application.h>
#include <engine/engine.h>
#include <graphics/window.h>
#include <core/vfs.h>
#include <logger/logger.h>
#include <core/string_buffer.h>
#include <iostream>

#include <core/profiling.h>
#include <stdio.h>

#include "../TheUnpluggingGame/playerInput.h"
#include "../TheUnpluggingGame/fpsController.h"
#include "../TheUnpluggingGame/bowWeapon.h"
#include "ecs/ecs.h"

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

void register_systems_and_components(World& world) {
	register_default_systems_and_components(world);

	world.add(new Store<PlayerInput>(1));
	world.add(new Store<FPSController>(10));
	world.add(new Store<Bow>(5));
	world.add(new Store<Arrow>(20));
}

#define BUILD_STANDALONE

int main() {
	//Window window;
	//window.title = "The Unplugging";
	//window.full_screen = false;
	//window.width = 1980;
	//window.height = 1080;
	
	//window.vSync = false;
	//window.init();

	StringView level = "C:\\Users\\User\\Desktop\\TopCCompiler\\TopCompiler\\Fernix\\assets\\level2\\";
	//StringView level = "assets\\level2\\";
	
	Engine engine;

	register_systems_and_components(engine.world);

	Level::set_level(level);

	StringView game_dll_path = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Debug\\TheUnpluggingGame.dll";
	StringView engine_dll_path = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Debug\\NextEngineEditor.dll";

	Application game(game_dll_path, &engine);
	game.init();

#ifdef BUILD_STANDALONE
	game.run();
#else
	Application editor(engine_dll_path, &engine);
	editor.init(&game);
	editor.run();

#endif

	return 0;
}