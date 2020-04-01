#pragma once

#include "core/handle.h"

struct World;
struct RenderCtx;
struct ShaderConfig;

struct ENGINE_API Pass {
	enum PassID { Scene, Shadow0, Shadow1, Shadow2, Shadow3, Count };
	enum PassType { Standard, Depth_Only };

	PassID id = Scene;
	PassType type = Standard;

	virtual void render(World&, RenderCtx&) {}
	virtual void set_shader_params(ShaderConfig&, RenderCtx&) {};
};