#include "visualization/render_backend.h"
#include "graphics/rhi/buffer.h"
#include "graphics/rhi/pipeline.h"
#include "graphics/rhi/draw.h"
#include "graphics/rhi/rhi.h"
#include "graphics/pass/pass.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/assets.h"
#include "core/time.h"
#include <glm/gtc/matrix_transform.hpp>

struct CFDRenderBackend {
    uint frame_index;

    Arena vertex_arena[MAX_FRAMES_IN_FLIGHT];
    Arena index_arena[MAX_FRAMES_IN_FLIGHT];
    
    CPUVisibleBuffer vertex_buffer;
    CPUVisibleBuffer index_buffer;
    
    VertexLayout triangle_vertex_layout;
    VertexLayout line_vertex_layout;
    VertexBuffer line_vertex_buffer;
    VertexBuffer solid_vertex_buffer;

    pipeline_handle pipeline_mesh_solid;
    pipeline_handle pipeline_mesh_wireframe;
    pipeline_handle pipeline_triangle;
    pipeline_handle pipeline_line;

    descriptor_set_handle frame_descriptor;
};

CFDRenderBackend* make_cfd_render_backend(const CFDRenderBackendOptions& options) {
    CFDRenderBackend* backend = PERMANENT_ALLOC(CFDRenderBackend);

    VertexLayoutDesc triangle_vertex_layout = {
        {
        {3, VertexAttrib::Float, offsetof(CFDTriangleVertex, position)},
        {3, VertexAttrib::Float, offsetof(CFDTriangleVertex, normal)},
        {4, VertexAttrib::Float, offsetof(CFDTriangleVertex, color)}
        }, sizeof(CFDTriangleVertex) 
    };

    VertexLayoutDesc line_vertex_layout = {
        {
        {3, VertexAttrib::Float, offsetof(CFDLineVertex, position)},
        {4, VertexAttrib::Float, offsetof(CFDLineVertex, color)}
        }, sizeof(CFDLineVertex)
    };

    backend->triangle_vertex_layout = register_vertex_layout(triangle_vertex_layout);
    backend->line_vertex_layout = register_vertex_layout(line_vertex_layout);

    alloc_Arena(backend->vertex_arena, options.vertex_buffer_size, &backend->vertex_buffer, BUFFER_VERTEX);
    alloc_Arena(backend->index_arena, options.index_buffer_size, &backend->index_buffer, BUFFER_INDEX);

    shader_flags flags = 0;
    shader_handle line_shader = load_Shader("shaders/CFD/line.vert", "shaders/CFD/line.frag", flags);
    shader_handle triangle_shader = load_Shader("shaders/CFD/triangle.vert", "shaders/CFD/triangle.frag", flags);

    GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.vertex_layout = VERTEX_LAYOUT_DEFAULT;
    pipeline_desc.instance_layout = INSTANCE_LAYOUT_NONE;
    pipeline_desc.shader = triangle_shader;
    pipeline_desc.shader_flags = flags;
    pipeline_desc.subpass = 1;
    pipeline_desc.render_pass = RenderPass::Scene;
    pipeline_desc.state = 0;
    pipeline_desc.vertex_layout = backend->triangle_vertex_layout;
    memset(pipeline_desc.range, 0, sizeof(pipeline_desc.range));
    backend->pipeline_triangle = query_Pipeline(pipeline_desc);

    pipeline_desc.shader = line_shader;
    pipeline_desc.state = Cull_None | PrimitiveType_LineList | (5 << WireframeLineWidth_Offset);
    pipeline_desc.vertex_layout = backend->line_vertex_layout;
    backend->pipeline_line = query_Pipeline(pipeline_desc);
    
    return backend;
}


template<typename Buffer, typename T>
void alloc_typed_buffer(CFDRenderBackend& backend, Buffer buffers[MAX_FRAMES_IN_FLIGHT], u64 vertex_size, u64 index_size) {
    for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        buffers[i].vertex_arena = backend.vertex_arena[i];
        buffers[i].index_arena = backend.index_arena[i];

        buffers[i].vertex_arena.base_offset += buffers[i].vertex_arena.offset;
        buffers[i].vertex_arena.offset = 0;

        buffers[i].index_arena.base_offset += buffers[i].index_arena.offset;
        buffers[i].index_arena.offset = 0;

        buffers[i].vertices = (T*)(backend.vertex_buffer.mapped + buffers[i].vertex_arena.base_offset);
        buffers[i].indices = (uint*)(backend.index_buffer.mapped + buffers[i].index_arena.base_offset);

        backend.vertex_arena[i].offset += vertex_size;
        backend.index_arena[i].offset += index_size;
    }
}

void alloc_triangle_buffer(CFDRenderBackend& backend, CFDTriangleBuffer buffers[MAX_FRAMES_IN_FLIGHT], u64 vertex_size, u64 index_size) {
    alloc_typed_buffer<CFDTriangleBuffer, CFDTriangleVertex>(backend, buffers, vertex_size, index_size);
}

void alloc_line_buffer(CFDRenderBackend& backend, CFDLineBuffer buffers[MAX_FRAMES_IN_FLIGHT], u64 vertex_size, u64 index_size) {
    alloc_typed_buffer<CFDLineBuffer, CFDLineVertex>(backend, buffers, vertex_size, index_size);
}

descriptor_set_handle get_frame_descriptor(CFDRenderBackend& backend) {
    return backend.frame_descriptor;
}

RenderPass begin_cfd_scene_pass(CFDRenderBackend& backend, Renderer& renderer, FrameData& frame) {
    uint frame_index = get_frame_index();

    SimulationUBO simulation_ubo = {};
    simulation_ubo.time = Time::now();
    memcpy_ubo_buffer(renderer.scene_pass_ubo[frame_index], &frame.pass_ubo);
    memcpy_ubo_buffer(renderer.simulation_ubo[frame_index], &simulation_ubo);

    RenderPass render_pass = begin_render_pass(RenderPass::Scene);
    next_subpass(render_pass);

    backend.frame_descriptor = renderer.scene_pass_descriptor[frame_index];

    return render_pass;
}

void render_triangle_buffer(CFDRenderBackend& backend, CommandBuffer& cmd_buffer, CFDTriangleBuffer& buffer, uint length) {
    bind_pipeline(cmd_buffer, backend.pipeline_triangle);
    bind_descriptor(cmd_buffer, 0, backend.frame_descriptor);
    bind_vertex_buffer(cmd_buffer, backend.vertex_buffer.buffer, buffer.vertex_arena.base_offset);
    bind_index_buffer(cmd_buffer, backend.index_buffer.buffer, buffer.index_arena.base_offset);
    draw_indexed(cmd_buffer, length, 1, 0, 0);
}

void render_triangle_buffer(CFDRenderBackend& backend, CommandBuffer& cmd_buffer, CFDTriangleBuffer& buffer) {
    render_triangle_buffer(backend, cmd_buffer, buffer, buffer.index_arena.offset);
}

void render_line_buffer(CFDRenderBackend& backend, CommandBuffer& cmd_buffer, CFDLineBuffer& line, uint length) {
    bind_pipeline(cmd_buffer, backend.pipeline_line);
    bind_descriptor(cmd_buffer, 0, backend.frame_descriptor);
    bind_vertex_buffer(cmd_buffer, backend.vertex_buffer.buffer, line.vertex_arena.base_offset);
    bind_index_buffer(cmd_buffer, backend.index_buffer.buffer, line.index_arena.base_offset);
    draw_indexed(cmd_buffer, line.index_arena.offset, 1, 0, 0);
}

void render_line_buffer(CFDRenderBackend& backend, CommandBuffer& cmd_buffer, CFDLineBuffer& line) {
    render_line_buffer(backend, cmd_buffer, line, line.index_arena.offset);
}

void CFDLineBuffer::draw_line(vec3 start, vec3 end, vec4 color) {
    uint offset = vertex_arena.offset;

    append({ vec4(start,0.0), color });
    append({ vec4(end,0.0), color });
    append(offset + 0);
    append(offset + 1);
}

void CFDTriangleBuffer::draw_triangle(vec3 v[3], vec3 normal, vec4 color) {
    uint offset = vertex_arena.offset;

    for (uint i = 0; i < 3; i++) {
        append({ vec4(v[i],0), vec4(normal, 1.0), color });
        append(offset + i);
    }
}

void CFDTriangleBuffer::draw_triangle(vec3 v0, vec3 v1, vec3 v2, vec4 color) {
    vec3 v[3] = { v0,v1,v2 };
    draw_triangle(v0, v1, v2, color);
}

void CFDTriangleBuffer::draw_quad(vec3 v[4], vec4 color) {
    uint offset = vertex_arena.offset;

    for (uint i = 0; i < 4; i++) {
        append({ vec4(v[i],0), vec4(vec3(0),0), color });
    }

    append(offset + 0);
    append(offset + 1);
    append(offset + 2);

    append(offset + 0);
    append(offset + 2);
    append(offset + 3);
}

void CFDTriangleBuffer::draw_quad(vec3 v0, vec3 v1, vec3 v2, vec3 v3, vec4 color) {
    vec3 v[4] = { v0,v1,v2,v3 };
    draw_quad(v, color);
}

void CFDTriangleBuffer::draw_quad(vec3 origin, vec2 size, vec3 rot, vec4 color) {
    vec3 v[4] = { vec3(-1,-1,0), vec3(1,-1,0), vec3(1,1,0), vec3(-1,1,0) };
    glm::mat4 mat = glm::rotate(glm::mat4(1.0f), 2.0f * glm::pi<float>(), glm::vec3(rot));

    for (uint i = 0; i < 4; i++) {
        v[i] = glm::vec3(mat * glm::vec4(glm::vec3(v[i]), 1.0f) * glm::vec4(size.x, size.y, 0, 1));
        v[i] += origin;
    }
    draw_quad(v, color);
}