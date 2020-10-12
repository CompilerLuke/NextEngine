#pragma once

#include "core/core.h"

REFL using ID = uint;
REFL using Layermask = u64;
REFL using Archetype = u64;
REFL using EntityFlags = u64;
using ComponentID = uint;

constexpr uint MAX_COMPONENTS = 64;
constexpr uint MAX_ENTITIES = 10000;
constexpr uint ARCHETYPE_HASH = 103;
constexpr uint BLOCK_SIZE = kb(8);
constexpr uint WORLD_SIZE = mb(50);

const Archetype ANY_ARCHETYPE = ~0ull;

struct EntityQuery {
	Archetype all = 0;
	Archetype some = 0;
	Archetype none = 0;

	template<typename... Args>
	EntityQuery with_all(EntityFlags flags = 0);

	template<typename... Args>
	EntityQuery with_none(EntityFlags flags = 0);

	template<typename... Args>
	EntityQuery with_some(EntityFlags flags = 0);
};


/*
constexpr Layermask GAME_LAYER = 1 << 0;
constexpr Layermask EDITOR_LAYER = 1 << 1;
constexpr Layermask PICKING_LAYER = 1 << 2;
constexpr Layermask SHADOW_LAYER = 1 << 3;

constexpr EntityFlags STATIC = 1 << 4;
constexpr EntityFlags CREATED_THIS_FRAME = 1 << 5;
constexpr EntityFlags DESTROYED_THIS_FRAME = 1 << 6;
constexpr EntityFlags MODIFIED_THIS_FRAME = 1 << 7;
*/

constexpr bool has_component(Archetype arch, ComponentID id) { return arch & (1ull << id); }