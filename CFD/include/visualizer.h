#pragma once

#include "engine/handle.h"
#include "graphics/rhi/buffer.h"

struct CommandBuffer;
struct CFDVolume;

struct CFDVisualization {
	VertexBuffer vertex_buffer;
	material_handle material;
};

void make_cfd_visualization(CFDVisualization& visualization);
void build_vertex_representation(CFDVisualization& visualization, CFDVolume&);
void render_cfd_mesh(CFDVisualization& visualization, CommandBuffer& cmd_buffer);