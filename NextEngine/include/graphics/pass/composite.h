#pragma once

#include "engine/handle.h"
#include "graphics/rhi/buffer.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

struct Viewport;

struct CompositeResources {
    UBOBuffer ubo[MAX_FRAMES_IN_FLIGHT];
    descriptor_set_handle descriptors[MAX_FRAMES_IN_FLIGHT];
    pipeline_handle pipeline;
    texture_handle composite_map;
};

struct CompositeUBO {
    glm::mat4 to_depth;
    float fog_begin;
    float fog_end;
    alignas(16) glm::vec3 fog_color;
};

void make_composite_resources(CompositeResources&, texture_handle depth_scene_map, texture_handle scene_map, texture_handle volumetric_map, texture_handle cloud_map, uint width, uint height);
void fill_composite_ubo(CompositeUBO& ubo, Viewport&);
void render_composite_pass(CompositeResources&);
