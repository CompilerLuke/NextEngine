#pragma once

#include "engine/handle.h"
#include "graphics/rhi/buffer.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

struct Viewport;

struct CompositeResources {
	pipeline_handle pipeline;
	descriptor_set_handle descriptor[MAX_FRAMES_IN_FLIGHT];
	UBOBuffer ubo[MAX_FRAMES_IN_FLIGHT];
	texture_handle composite_map;
};

struct CompositeUBO {
	glm::mat4 depth_proj;
	glm::vec3 fog_color;
	float fog_begin;
};

class RenderSubmitCtx;

void make_composite_resources(CompositeResources&, texture_handle depth, texture_handle scene, texture_handle volumetric, texture_handle cloud, uint width, uint height);
void fill_composite_ubo(CompositeUBO& ubo, Viewport&);
void render_composite_pass(CompositeResources&, RenderSubmitCtx&);
