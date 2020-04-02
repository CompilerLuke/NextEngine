#pragma once

#include "core/core.h"
#include "core/container/string_view.h"
#include "core/io/vfs.h"

struct Engine {
	Level level;
	struct Window& window;
	struct Time& time;
	struct World& world;
	struct Input& input;
	struct Renderer& renderer;
	struct AssetManager& asset_manager;
	struct PhysicsSystem& physics_system;
	struct LocalTransformSystem& local_transforms_system;

	ENGINE_API Engine(string_view);
	ENGINE_API ~Engine();
	ENGINE_API void begin_frame();
	ENGINE_API void end_frame();
};