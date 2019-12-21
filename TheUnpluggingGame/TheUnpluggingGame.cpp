// TheUnpluggingGame.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "game.h"

#include <ecs/system.h>
#include <components/transform.h>
#include <model/model.h>
#include <graphics/rhi.h>

#include "playerInput.h"
#include "fpsController.h"
#include "bowWeapon.h"

#include "reflection/reflection.h"
#include "components/transform.h"
#include "engine/application.h"
#include "engine/engine.h"

struct Game {
	Engine& engine;

	PlayerInputSystem player_input;
	FPSControllerSystem fps_controller;
	BowSystem bow_system;
};

APPLICATION_API Game* init(Engine& engine) {
	Game* game = new Game{ engine };
	return game;
}

APPLICATION_API bool is_running(Game& game) {
	return !game.engine.window.should_close();
}

APPLICATION_API void update(Game& game) {
	World& world = game.engine.world;
	UpdateParams params(game.engine.input);

	game.player_input.update(world, params);
	game.fps_controller.update(world, params);
	game.bow_system.update(world, params);
}

APPLICATION_API void render(Game& game) {
}

APPLICATION_API void deinit(Game* game) {
	delete game;
}

extern "C" {
	void game_systems(World& world) {
		world.add(new PlayerInputSystem());
		world.add(new FPSControllerSystem());
		world.add(new BowSystem());
	}
}