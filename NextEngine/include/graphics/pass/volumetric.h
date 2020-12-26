#pragma once

#include "engine/handle.h"
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include "ecs/id.h"
#include "graphics/rhi/buffer.h"

struct ShadowProjInfo;
struct ShadowResources;
struct World;
struct Viewport;
struct CompositeUBO;

REFL
struct VolumetricSettings {
	uint fog_steps = 25;
	uint fog_cloud_shadow_steps = 2;
	uint cloud_steps = 40;
	uint cloud_shadow_steps = 5;
};

COMP 
struct CloudVolume {
	glm::vec3 size = glm::vec3(100,100,100);
	float phase = 0.9;
	float light_absorbtion_in_cloud = 0.1;
	float light_absorbtion_towards_sun = 0.1;
	float forward_scatter_intensity = 1.0;
	float shadow_darkness_threshold = 0.1;
	glm::vec3 wind;
};

COMP 
struct FogVolume {
	glm::vec3 size = glm::vec3(100,100,100);
	float coefficient = 0.8;
	glm::vec3 forward_scatter_color = glm::vec3(1);
	float intensity = 1;
	glm::vec3 fog_color = glm::vec3(66.0f/200, 188.0f/200, 245.0/200);
	float fog_begin;
};

struct VolumetricUBO {
	alignas(16) glm::mat4 to_world;
	alignas(16) glm::vec3 cam_position;	
	alignas(16) glm::vec3 sun_position;
	alignas(16) glm::vec3 sun_direction;
	alignas(16) glm::vec3 sun_color;
	
	int fog_steps;	
	int fog_cloud_shadow_steps;
	int cloud_steps;
	int cloud_shadow_steps;

	alignas(16) glm::vec3 scatterColor;
	float fog_coefficient;
	float intensity;

	float cloud_density;
	float cloud_bottom;
	float cloud_top;
	float cloud_phase;
	float light_absorbtion_in_cloud;
	float light_absorbtion_towards_sun;
	float forward_intensity;
	float shadow_darkness_threshold;
	
	alignas(16) glm::vec3 wind;
	float time;
};

struct VolumetricResources {
	texture_handle depth_prepass;
	texture_handle volumetric_map;
	texture_handle cloud_map;
	UBOBuffer ubo[MAX_FRAMES_IN_FLIGHT];
	descriptor_set_handle descriptor[MAX_FRAMES_IN_FLIGHT];
	pipeline_handle pipeline;

	shader_handle volume_shader;
};

void make_volumetric_resources(VolumetricResources&, texture_handle depthprepass_map, ShadowResources& shadow, uint width, uint height);
void fill_volumetric_ubo(VolumetricUBO&, CompositeUBO& composite, World&, VolumetricSettings& settings, Viewport& viewport, EntityQuery);
void render_volumetric_pass(VolumetricResources&, const VolumetricUBO& ubo);
