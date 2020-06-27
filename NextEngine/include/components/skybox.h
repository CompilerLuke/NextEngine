#pragma once

#include "core/handle.h"

struct Skybox {
	cubemap_handle cubemap;

	REFLECT()
};

ENGINE_API uint make_default_Skybox(World&, string_view);
