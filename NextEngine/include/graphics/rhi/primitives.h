#pragma once

#include "engine/engine.h"
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
ENGINE_API void draw_quad(struct CommandBuffer&);

void init_primitives();

