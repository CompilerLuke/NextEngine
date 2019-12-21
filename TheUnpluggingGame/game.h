#pragma once

#include <ecs/ecs.h>
#include <core/core.h>

extern "C" {
	void ENGINE_API register_components_and_systems(World&);
	void ENGINE_API game_systems(World&);
}