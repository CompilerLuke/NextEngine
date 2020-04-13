#pragma once

#include "core/core.h"

struct ENGINE_API RenderFeature {
	virtual void pre_render(struct World& world, struct PreRenderParams&) {}
	virtual void render(struct World& world, struct RenderCtx& params) {}
	virtual ~RenderFeature() {}
};