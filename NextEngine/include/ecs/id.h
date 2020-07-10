#pragma once

#include "core/core.h"
#include "core/reflection.h"

using ID = uint;
using Layermask = uint;
using Flags = uint;

constexpr uint MAX_COMPONENTS = 64;
constexpr uint MAX_ENTITIES = 10000;
constexpr uint ARCHETYPE_HASH = 103;
constexpr uint BLOCK_SIZE = kb(8);
constexpr uint WORLD_SIZE = mb(50);
using Archetype = u64;
using ComponentID = uint;

const Archetype ANY_ARCHETYPE = ~0ull;