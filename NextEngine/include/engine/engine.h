#pragma once

#include "core.h"
#include "core/container/string_view.h"

struct ENGINE_API Modules {
	struct Window* window = nullptr;
	struct Time* time = nullptr;
	struct World* world = nullptr;
	struct Input* input = nullptr;
	struct Renderer* renderer = nullptr;
	struct PhysicsSystem* physics_system = nullptr;
	struct LocalTransformSystem* local_transforms_system = nullptr;

    Modules();
	Modules(const char* app_name, const char* level_path, const char* engine_asset_path);
	void begin_frame();
	void end_frame();
	~Modules();
};
