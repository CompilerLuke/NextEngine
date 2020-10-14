#pragma once

#include "engine/handle.h"
#include "graphics/rhi/buffer.h"

struct Primitives {
	model_handle quad;
	model_handle cube;
	model_handle sphere;

	VertexBuffer quad_buffer;
	VertexBuffer cube_buffer;
	VertexBuffer sphere_buffer;
};

ENGINE_API extern Primitives primitives;
ENGINE_API void render_quad();

void init_primitives();

