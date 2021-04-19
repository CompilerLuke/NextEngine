#pragma once

#include "core/core.h"
#include "core/math/vec2.h"
#include "core/math/vec3.h"
#include "core/math/vec4.h"
#include "core/container/slice.h"
#include "engine/handle.h"

#include "graphics/rhi/buffer.h" //todo: extract Arena struct
#include "graphics/pass/pass.h"

struct Arena;
struct CFDRenderBackend;
struct CommandBuffer;
struct Renderer;
struct FrameData;

struct CFDTriangleVertex {
    vec4 position;
    vec4 normal;
    vec4 color;
    vec4 padding;
};

struct CFDLineVertex {
    vec4 position;
    vec4 color;
};

struct CFDRenderBackendOptions {
    u64 vertex_buffer_size = mb(300);
    u64 index_buffer_size = mb(300);
};

struct CFDTriangleBuffer {
    Arena vertex_arena = {};
    Arena index_arena = {};
    CFDTriangleVertex* vertices = nullptr;
    uint* indices = nullptr;

    inline void append(CFDTriangleVertex v) { vertices[vertex_arena.offset++] = v; }
    inline void append(uint i) { indices[index_arena.offset++] = i;}
    inline void clear() { vertex_arena.offset = 0; index_arena.offset = 0; }

    void draw_triangle(vec3 v[3], vec3 normal, vec4 color);
    void draw_triangle(vec3 v0, vec3 v1, vec3 v2, vec4 color);
    void draw_quad(vec3 v[4], vec4 color);
    void draw_quad(vec3 v0, vec3 v1, vec3 v2, vec3 v3, vec4 color);
    void draw_quad(vec3 origin, vec2 size, vec3 rot, vec4 color);
};

struct CFDLineBuffer {
    Arena vertex_arena = {};
    Arena index_arena = {};
    CFDLineVertex* vertices = nullptr;
    uint* indices = nullptr;

    inline void append(CFDLineVertex v) { vertices[vertex_arena.offset++] = v; }
    inline void append(uint i) { indices[index_arena.offset++] = i; }
    inline void clear() { vertex_arena.offset = 0; index_arena.offset = 0; }

    void draw_line(vec3 start, vec3 end, vec4 color);
};

CFDRenderBackend* make_cfd_render_backend(const CFDRenderBackendOptions&);
void destroy_cfd_render_backend(CFDRenderBackend*);

void alloc_triangle_buffer(CFDRenderBackend& backend, CFDTriangleBuffer buffers[MAX_FRAMES_IN_FLIGHT], u64 vertex_size, u64 index_size);
void alloc_line_buffer(CFDRenderBackend& backend, CFDLineBuffer buffers[MAX_FRAMES_IN_FLIGHT], u64 vertex_size, u64 index_size);

descriptor_set_handle get_frame_descriptor(CFDRenderBackend&);

void render_triangle_buffer(CFDRenderBackend& backend, CommandBuffer&, CFDTriangleBuffer&, uint length);
void render_triangle_buffer(CFDRenderBackend& backend, CommandBuffer&, CFDTriangleBuffer&);
void render_line_buffer(CFDRenderBackend& backend, CommandBuffer&, CFDLineBuffer&, uint length);
void render_line_buffer(CFDRenderBackend& backend, CommandBuffer&, CFDLineBuffer&);

RenderPass begin_cfd_scene_pass(CFDRenderBackend&, Renderer& renderer, FrameData& frame);
