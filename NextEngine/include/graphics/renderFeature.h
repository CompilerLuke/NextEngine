#pragma once

struct RenderFeature {
	virtual void pre_render(struct World& world, struct PreRenderParams&) {}
	virtual void render(struct World& world, struct RenderParams& params) {}
	virtual ~RenderFeature() {}
};