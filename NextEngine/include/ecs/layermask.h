#pragma once

#include "id.h"

constexpr Layermask game_layer = 1 << 0;
constexpr Layermask editor_layer = 1 << 1;
constexpr Layermask picking_layer = 1 << 2;
constexpr Layermask any_layer = ~0;