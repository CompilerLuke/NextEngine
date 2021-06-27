#include "visualization/debug_renderer.h"
#include "visualization/render_backend.h"
#include "core/container/vector.h"
#include <glm/mat4x4.hpp>
#include <glm/mat3x3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/job_system/job.h"

struct ColorInstance {
    glm::mat4 model;
    vec3 color;
};

struct CFDDebugRenderer {
    CFDRenderBackend& backend;
    CFDTriangleBuffer triangles[MAX_FRAMES_IN_FLIGHT];
    CFDLineBuffer lines[MAX_FRAMES_IN_FLIGHT];
    uint frame_index;
    atomic_counter counter;
};

CFDDebugRenderer* make_cfd_debug_renderer(CFDRenderBackend& backend) {
    CFDDebugRenderer* renderer = new CFDDebugRenderer{backend};
    alloc_triangle_buffer(backend, renderer->triangles, mb(50), mb(50));
    alloc_line_buffer(backend, renderer->lines, mb(10), mb(10));

    renderer->counter = INT_MAX;

    return renderer;
}

void destroy_cfd_debug_renderer(CFDDebugRenderer* renderer) {
    delete renderer;
}

void draw_line(CFDDebugRenderer& renderer, vec3 start, vec3 end, vec4 color) {
    auto& lines = renderer.lines[renderer.frame_index];
    lines.draw_line(start, end, color);
}

vec3 triangle_normal(vec3 v[3]);

void draw_triangle(CFDDebugRenderer& renderer, vec3 v[3], vec4 color) {
    auto& triangles = renderer.triangles[renderer.frame_index];
    auto& lines = renderer.lines[renderer.frame_index];
    
    vec3 normal = triangle_normal(v);
    triangles.draw_triangle(v, normal, color); //no normal needed, flat shading

    vec3 dt = normal * 0.005;

    lines.draw_line(v[0] + dt, v[1] + dt, vec4(0,0,0,1));
    lines.draw_line(v[1] + dt, v[2] + dt, vec4(0, 0, 0, 1));
    lines.draw_line(v[2] + dt, v[0] + dt, vec4(0, 0, 0, 1));
}

void draw_triangle(CFDDebugRenderer& renderer, vec3 v0, vec3 v1, vec3 v2, vec4 color) {
    vec3 v[3] = { v0,v1,v2 };
    draw_triangle(renderer, v, color);
}

void draw_quad(CFDDebugRenderer& renderer, vec3 v[4], vec4 color) {
    auto& triangles = renderer.triangles[renderer.frame_index];
    triangles.draw_quad(v, color);
}

void draw_quad(CFDDebugRenderer& renderer, vec3 v0, vec3 v1, vec3 v2, vec3 v3, vec4 color) {
    vec3 v[4] = { v0,v1,v2,v3 };
    draw_quad(renderer, v, color);
}

void draw_quad(CFDDebugRenderer& renderer, vec3 origin, vec2 size, vec3 rot, vec4 color) {
    vec3 v[4] = { vec3(-1,-1,0), vec3(1,-1,0), vec3(1,1,0), vec3(-1,1,0) };
    glm::mat4 mat = glm::rotate(glm::mat4(1.0f), 2.0f*glm::pi<float>(), glm::vec3(rot));

    for (uint i = 0; i < 4; i++) {
        v[i] = glm::vec3(mat * glm::vec4(glm::vec3(v[i]),1.0f) * glm::vec4(size.x,size.y,0,1));
        v[i] += origin;
    }
    draw_quad(renderer, v, color);
}

void draw_point(CFDDebugRenderer& debug, vec3 pos, vec4 color) {
    float dt = 0.01;

    draw_line(debug, pos - vec3(0, dt, 0), pos + vec3(0, dt, 0), color);
    draw_line(debug, pos - vec3(dt, 0, 0), pos + vec3(dt, 0, 0), color);
    draw_line(debug, pos - vec3(0, 0, dt), pos + vec3(0, 0, dt), color);
}

void suspend_execution(CFDDebugRenderer& renderer) {
    renderer.counter++;
    wait_for_counter(&renderer.counter, INT_MAX);
}

void resume_execution(CFDDebugRenderer& renderer, uint value) {
    renderer.counter -= value;
}

void clear_debug_stack(CFDDebugRenderer& renderer) {
    renderer.frame_index = (renderer.frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
    renderer.lines[renderer.frame_index].vertex_arena.offset = 0;
    renderer.lines[renderer.frame_index].index_arena.offset = 0;
    renderer.triangles[renderer.frame_index].vertex_arena.offset = 0;
    renderer.triangles[renderer.frame_index].index_arena.offset = 0;
}

void prev_debug_stack(CFDDebugRenderer& renderer) {

}

void next_debug_stack(CFDDebugRenderer& renderer) {

}

void render_debug(CFDDebugRenderer& renderer, CommandBuffer& cmd_buffer) {
    CFDRenderBackend& backend = renderer.backend;
    auto& triangles = renderer.triangles[renderer.frame_index];
    auto& lines = renderer.lines[renderer.frame_index];

    render_triangle_buffer(backend, cmd_buffer, triangles, triangles.index_arena.offset);
    render_line_buffer(backend, cmd_buffer, lines, lines.index_arena.offset);
}
