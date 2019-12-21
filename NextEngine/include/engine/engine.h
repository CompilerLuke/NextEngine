#pragma once

#include "core/core.h"
#include "ecs/ecs.h"
#include "graphics/renderer.h"
#include "core/input.h"
#include "graphics/window.h"
#include "core/game_time.h"

struct Engine {
	Window window;
	Renderer renderer;
	Input input;
	Time time;
	World world;

	ENGINE_API Engine();
};