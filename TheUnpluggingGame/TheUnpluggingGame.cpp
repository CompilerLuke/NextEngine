// TheUnpluggingGame.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "game.h"

#include <ecs/system.h>
#include <components/transform.h>
#include <graphics/assets/model.h>
#include <graphics/rhi/window.h>

#include <core/io/input.h>
#include "playerInput.h"
#include "fpsController.h"
#include "bowWeapon.h"

#include "core/reflection.h"
#include "components/transform.h"
#include "engine/application.h"
#include "engine/engine.h"
#include "physics/physics.h"

struct Time;
struct World;

struct Game {};

APPLICATION_API Game* init(Modules& engine) {
	return new Game{};
}

APPLICATION_API bool is_running(Game& game, Modules& modules) {
	return !modules.window->should_close();
}

APPLICATION_API void register_components(World& world) {
	world.add_component<PlayerInput>();
	world.add_component<FPSController>();
	world.add_component<Bow>();
	world.add_component<Arrow>();
}

APPLICATION_API void update(Game& game, Modules& modules) {
	World& world = *modules.world;
	UpdateCtx ctx(*modules.time, *modules.input);
	ctx.layermask = GAME_LAYER;

	modules.input->capture_mouse(true);
	update_player_input(world, ctx);
	update_fps_controllers(world, ctx);
	update_bows(world, ctx);

	modules.local_transforms_system->update(world, ctx);
	modules.physics_system->update(world, ctx);
}

APPLICATION_API void render(Game& game, Modules& engine) {
}

APPLICATION_API void deinit(Game* game, Modules& engine) {
	delete game;
}