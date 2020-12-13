#pragma once

#include "engine/handle.h"
#include "graphics/rhi/buffer.h"
#include "ecs/id.h"

struct Camera;
struct DirLight;
struct Transform;

const uint MAX_SHADOW_CASCADES = 4;

struct ShadowResources {
	texture_handle cascade_maps[MAX_SHADOW_CASCADES];
	UBOBuffer cascade_ubos[MAX_FRAMES_IN_FLIGHT][MAX_SHADOW_CASCADES];
	descriptor_set_handle cascade_descriptors[MAX_FRAMES_IN_FLIGHT][MAX_SHADOW_CASCADES];
	UBOBuffer shadow_ubos[MAX_FRAMES_IN_FLIGHT];
	pipeline_layout_handle cascade_layout;
	sampler_handle shadow_sampler;
};

struct ShadowCascadeProj {
	float end_distance;
	float end_clip_space;
	glm::mat4 to_light;
	glm::mat4 ndc_to_light;
	glm::mat4 light_projection;
	glm::mat4 light_view; //todo same for all cascades, could be moved out
	glm::mat4 round;
};

struct ShadowUBO {
	alignas(16) glm::mat4 to_light_space[MAX_SHADOW_CASCADES];
	alignas(16) glm::vec4 cascade_end_clipspace[MAX_SHADOW_CASCADES]; //todo more space efficient layout
};

struct ShadowSettings {
	float shadow_resolution;
	float cascade_split_lambda = 0.96;
	float constant_depth_bias = 1.5;
	float slope_depth_bias = 2.5;
};

void make_shadow_resources(ShadowResources&, UBOBuffer simulation_ubo[MAX_FRAMES_IN_FLIGHT], const ShadowSettings& settings);
void add_shadow_descriptors(DescriptorDesc& desc, ShadowResources& shadow, uint frame);
void extract_shadow_cascades(ShadowCascadeProj cascades[MAX_SHADOW_CASCADES], Viewport viewports[], const ShadowSettings& settings, World& world, Viewport& viewport, EntityQuery query);

void fill_shadow_ubo(ShadowUBO& shadow_ubo, const ShadowCascadeProj info[MAX_SHADOW_CASCADES]);
void bind_cascade_viewports(ShadowResources& resources, RenderPass render_pass[MAX_SHADOW_CASCADES], const ShadowSettings& settings, const ShadowCascadeProj info[MAX_SHADOW_CASCADES]);
