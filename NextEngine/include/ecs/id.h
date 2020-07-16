#pragma once

#include "core/core.h"
#include "core/reflection.h"

REFL using ID = uint;
REFL using Layermask = uint;
REFL using Flags = uint;

constexpr uint MAX_COMPONENTS = 64;
constexpr uint MAX_ENTITIES = 10000;
constexpr uint ARCHETYPE_HASH = 103;
constexpr uint BLOCK_SIZE = kb(8);
constexpr uint WORLD_SIZE = mb(50);

REFL using Archetype = u64;
using ComponentID = uint;

const Archetype ANY_ARCHETYPE = ~0ull;


constexpr Layermask GAME_LAYER = 1 << 0;
constexpr Layermask EDITOR_LAYER = 1 << 1;
constexpr Layermask PICKING_LAYER = 1 << 2;
constexpr Layermask SHADOW_LAYER = 1 << 3;
constexpr Layermask ANY_LAYER = ~0;

constexpr Flags STATIC = 1 << 0;