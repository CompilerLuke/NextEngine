#pragma once

#pragma once
#include "core/handle.h"

COMP
struct Skybox {
	cubemap_handle cubemap;
};

struct World;
ENGINE_API uint make_default_Skybox(World&, string_view);
