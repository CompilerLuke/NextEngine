#pragma once

#include "core/core.h"
#include "core/container/string_view.h"

struct ENGINE_API Modules {
	struct Level* level;
	struct Window* window;
	struct Time* time;
	struct World* world;
	struct Input* input;
	struct Renderer* renderer;
	struct AssetManager* asset_manager;
	struct PhysicsSystem* physics_system;
	struct LocalTransformSystem* local_transforms_system;
	struct RHI* rhi;

	Modules(const char* app_name, const char* level_path);
	void begin_frame();
	void end_frame();
	~Modules();
};