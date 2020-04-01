#pragma once

#include "core/core.h"
#include "core/container/string_view.h"

struct Engine {
	struct Window& window;
	struct Level& level;
	struct Time& time;
	struct World& world;
	struct Input& input;
	struct Renderer& renderer;
	struct AssetManager& asset_manager;
	
	ENGINE_API Engine(string_view);
	ENGINE_API ~Engine();
	ENGINE_API void begin_frame();
	ENGINE_API void end_frame();
};