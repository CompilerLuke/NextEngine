#pragma once


#include "core/handle.h"
#include "pipeline.h"
#include "core/container/tvector.h"
#include "graphics/rhi/shader_access.h"

struct CommandBuffer;

ENGINE_API CommandBuffer& begin_draw_cmds();
ENGINE_API void end_draw_cmds(CommandBuffer&);

ENGINE_API void draw_mesh(CommandBuffer&, model_handle, slice<material_handle>, struct Transform&);
ENGINE_API void draw_mesh(CommandBuffer&, model_handle, slice<material_handle>, slice<glm::mat4>);
ENGINE_API void draw_mesh(CommandBuffer&, model_handle, slice<material_handle>, glm::mat4);

ENGINE_API void push_constant(CommandBuffer& cmd_buffer, Stage stage, uint offset, uint size, const void* ptr);

template<typename T>
void push_constant(CommandBuffer& cmd_buffer, Stage stage, uint offset, const T* ptr) {
	push_constant(cmd_buffer, stage, offset, sizeof(T), ptr);
}

ENGINE_API void bind_vertex_buffer(CommandBuffer&, VertexLayout, InstanceLayout);
ENGINE_API void bind_descriptor(CommandBuffer&, uint, slice<descriptor_set_handle>);
ENGINE_API void bind_pipeline_layout(CommandBuffer&, pipeline_layout_handle);
ENGINE_API void bind_pipeline(CommandBuffer&, pipeline_handle);
ENGINE_API void bind_material(CommandBuffer&, material_handle);
ENGINE_API void draw_mesh(CommandBuffer&, VertexBuffer, InstanceBuffer);
ENGINE_API void draw_mesh(CommandBuffer&, VertexBuffer);

//void begin_render_pass(CommandBuffer&, render_pass_handle);
//void end_render_pass(CommandBuffer&);