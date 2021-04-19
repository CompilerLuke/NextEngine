#pragma once


#include "engine/handle.h"
#include "pipeline.h"
#include "core/container/tvector.h"
#include "graphics/rhi/shader_access.h"

struct Rect2D {
    glm::vec2 pos;
    glm::vec2 size;
    
    bool intersects(glm::vec2 point) {
        return point.x >= pos.x && point.y >= pos.y && point.x <= pos.x + size.x && point.y <= pos.y + size.y;
    }
};

struct CommandBuffer;

ENGINE_API CommandBuffer& begin_draw_cmds();
ENGINE_API void end_draw_cmds(CommandBuffer&);

ENGINE_API void draw_mesh(CommandBuffer&, model_handle, slice<material_handle>, struct Transform&, uint lod = 0);
ENGINE_API void draw_mesh(CommandBuffer&, model_handle, slice<material_handle>, slice<glm::mat4>, uint lod = 0);
ENGINE_API void draw_mesh(CommandBuffer&, model_handle, slice<material_handle>, glm::mat4, uint lod = 0);

ENGINE_API void push_constant(CommandBuffer& cmd_buffer, Stage stage, uint offset, uint size, const void* ptr);

template<typename T>
void push_constant(CommandBuffer& cmd_buffer, Stage stage, uint offset, const T* ptr) {
	push_constant(cmd_buffer, stage, offset, sizeof(T), ptr);
}

ENGINE_API void bind_vertex_buffer(CommandBuffer&, VertexLayout, InstanceLayout);
ENGINE_API void bind_vertex_buffer(CommandBuffer&, buffer_handle, u64 base);
ENGINE_API void bind_index_buffer(CommandBuffer&, buffer_handle, u64 base);
ENGINE_API void bind_descriptor(CommandBuffer&, uint, slice<descriptor_set_handle>);
ENGINE_API void bind_pipeline_layout(CommandBuffer&, pipeline_layout_handle);
ENGINE_API void bind_pipeline(CommandBuffer&, pipeline_handle);
ENGINE_API void bind_material(CommandBuffer&, material_handle);
ENGINE_API void bind_material_and_pipeline(CommandBuffer&, material_handle);
ENGINE_API void draw_mesh(CommandBuffer&, VertexBuffer, InstanceBuffer);
ENGINE_API void draw_mesh(CommandBuffer&, VertexBuffer);
ENGINE_API void draw_indexed(CommandBuffer& cmd_buffer, uint index_count, uint instance, uint index_base, uint vertex_base);
ENGINE_API void set_depth_bias(CommandBuffer&, float constant, float slope);
ENGINE_API void set_scissor(CommandBuffer&, Rect2D);


//void begin_render_pass(CommandBuffer&, render_pass_handle);
//void end_render_pass(CommandBuffer&);

/*
//Deferred API
#include "buffer.h"

struct DeferCommandBuffer;

ENGINE_API DeferCommandBuffer& alloc_cmd_buffer(Allocator* allocator = nullptr);

struct IndexedDrawCmd;

ENGINE_API IndexedDrawCmd& draw_cmd(DeferCommandBuffer&);
ENGINE_API void render_to_pass();

ENGINE_API void cmd_set_scissor();
*/
