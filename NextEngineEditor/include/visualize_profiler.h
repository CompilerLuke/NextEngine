#pragma once

struct VisualizeProfiler {
	hash_set<sstring, 103> name_to_color_idx;

	void render(struct World& world, struct Editor& editor, struct RenderPass& params);
};