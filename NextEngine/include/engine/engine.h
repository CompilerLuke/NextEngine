#pragma once

#include "core.h"
#include "core/container/string_view.h"

struct ENGINE_API Modules {
	struct Window* window;
	struct Time* time;
	struct World* world;
	struct Input* input;
	struct Renderer* renderer;
	struct PhysicsSystem* physics_system;
	struct LocalTransformSystem* local_transforms_system;

	Modules(const char* app_name, const char* level_path, const char* engine_asset_path);
	void begin_frame();
	void end_frame();
	~Modules();
};
