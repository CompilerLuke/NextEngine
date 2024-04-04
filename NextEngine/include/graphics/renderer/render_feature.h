#pragma once

#include "engine/core.h"

struct ENGINE_API RenderFeature {
	virtual void pre_render(struct World& world, struct PreRenderParams&) {}
	virtual void render(struct World& world, struct RenderPass& params) {}
	virtual ~RenderFeature() {}
};