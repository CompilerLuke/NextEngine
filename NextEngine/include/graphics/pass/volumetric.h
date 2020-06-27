#pragma once

#include "graphics/rhi/frame_buffer.h"
#include "graphics/assets/shader.h"

struct Assets;

struct FogMap {
	Framebuffer fbo;
	texture_handle map;

	FogMap(uint, uint);
};

struct ShadowParams {
	texture_handle depth_map;
	glm::vec2 in_range;
	glm::mat4 to_light;
	glm::mat4 to_world;
	unsigned int cascade = 0;

	ShadowParams() {};
};

struct VolumetricPass {
	texture_handle depth_prepass;
	FogMap calc_fog;

	shader_handle volume_shader;
	shader_handle upsample_shader;

	VolumetricPass(glm::vec2, texture_handle);
	void clear();

	//void render_with_cascade(struct World&, struct RenderPass&, ShadowParams& shadow_params);
	//void render_upsampled(struct World&, texture_handle, glm::mat4& proj_matrix);
};

void render_upsampled(VolumetricPass& pass, texture_handle, glm::mat4& proj_matrix);