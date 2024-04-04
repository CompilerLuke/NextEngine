#pragma once

#include "core/container/sstring.h"
#include "core/container/hash_map.h"

struct VisualizeProfiler {
	hash_set<sstring, 103> name_to_color_idx;
	float frame_max_time = 1.0 / 55.0;

	void render(struct World& world, struct Editor& editor, struct RenderPass& params);
};
