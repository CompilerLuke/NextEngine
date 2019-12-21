#pragma once

#include "renderFeature.h"

struct GrassRenderSystem : RenderFeature {
	void render(struct World&, struct RenderParams&) override;
};