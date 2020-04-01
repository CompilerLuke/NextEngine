#pragma once

#include "id.h"

constexpr Layermask GAME_LAYER = 1 << 0;
constexpr Layermask EDITOR_LAYER = 1 << 1;
constexpr Layermask PICKING_LAYER = 1 << 2;
constexpr Layermask SHADOW_LAYER = 1 << 3;
constexpr Layermask ANY_LAYER = ~0;

constexpr Flags STATIC = 1 << 0;