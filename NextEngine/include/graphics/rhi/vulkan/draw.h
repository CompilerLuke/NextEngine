#pragma once

#include "core/handle.h"
#include "buffer.h"
#include "pipeline.h"

struct CommandBuffer {
	VkCommandBuffer cmd_buffer;
	RenderPass::ID render_pass;

	VertexLayout bound_vertex_layout = (VertexLayout)-1; //VERTEX_LAYOUT_DEFAULT;
	InstanceLayout bound_instance_layout = (InstanceLayout)-1; // INSTANCE_LAYOUT_MAT4X4;
	pipeline_layout_handle bound_pipeline_layout = { INVALID_HANDLE };
	pipeline_handle bound_pipeline = { INVALID_HANDLE };
	material_handle bound_material = { INVALID_HANDLE };

	inline operator VkCommandBuffer() { return cmd_buffer; }
};