#pragma once

#include "core/handle.h"

struct Pass {
	virtual void render(struct World&, struct RenderParams&) {}
	virtual void set_shader_params(Handle<struct Shader>, struct World&, struct RenderParams&) {};
};