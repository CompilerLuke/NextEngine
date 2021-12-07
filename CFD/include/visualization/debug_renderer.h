#pragma once

#include "core/math/vec2.h"
#include "core/math/vec3.h"
#include "core/math/vec4.h"

struct CFDDebugRenderer;
struct CFDRenderBackend;
struct UI;
struct CommandBuffer;

CFDDebugRenderer* make_cfd_debug_renderer(CFDRenderBackend&);
void destroy_cfd_debug_renderer(CFDDebugRenderer*);

const vec4 DEFAULT_DEBUG_COLOR = vec4(vec3(1), 1);
const vec4 RED_DEBUG_COLOR = vec4(1, 0, 0, 1);
const vec4 GREEN_DEBUG_COLOR = vec4(0, 1, 0, 1);
const vec4 BLUE_DEBUG_COLOR = vec4(0, 0, 1, 1);

void draw_point(CFDDebugRenderer& debug, vec3 pos, vec4 color);
void draw_line(CFDDebugRenderer&, vec3 start, vec3 end, vec4 color = vec4(vec3(0),1));
void draw_triangle(CFDDebugRenderer&, vec3 v0, vec3 v1, vec3 v2, vec4 color = DEFAULT_DEBUG_COLOR);
void draw_triangle(CFDDebugRenderer&, vec3 v[3], vec4 color = DEFAULT_DEBUG_COLOR);
void draw_quad(CFDDebugRenderer&, vec3 v0, vec3 v1, vec3 v2, vec3 v3, vec4 color = DEFAULT_DEBUG_COLOR);
void draw_quad(CFDDebugRenderer&, vec3 v[4], vec4 color = DEFAULT_DEBUG_COLOR);
void draw_quad(CFDDebugRenderer&, vec3 origin, vec2 size, vec3 rot = vec3(0,0,0), vec4 color = DEFAULT_DEBUG_COLOR);
void draw_sphere(CFDDebugRenderer&, vec3 origin, float r, vec4 color = DEFAULT_DEBUG_COLOR);

void suspend_execution(CFDDebugRenderer&);
void suspend_and_reset_execution(CFDDebugRenderer&);
void resume_execution(CFDDebugRenderer&, uint value = 1);

void clear_debug_stack(CFDDebugRenderer&);
void prev_debug_stack(CFDDebugRenderer&);
void next_debug_stack(CFDDebugRenderer&);

void render_debug(CFDDebugRenderer&, CommandBuffer& cmd_buffer);

vec3 triangle_normal(vec3 v[3]);
vec3 quad_normal(vec3 v[4]);

