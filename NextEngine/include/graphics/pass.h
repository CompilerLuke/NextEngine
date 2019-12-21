#pragma once

#include "core/handle.h"

struct ENGINE_API Pass {
	enum PassType { Standard, Depth_Only };

	PassType type = Standard;

	virtual void render(struct World&, struct RenderParams&) {}
	virtual void set_shader_params(Handle<struct Shader>, Handle<struct ShaderConfig>, struct World&, struct RenderParams&) {};
};