#pragma once

#include "frameBuffer.h"
#include "shader.h"

struct FogMap {
	Framebuffer fbo;
	Handle<struct Texture> map;

	FogMap(unsigned int, unsigned int);
};

struct ShadowParams {
	Handle<struct Texture> depth_map;
	glm::vec2 in_range;
	glm::mat4 to_light;
	glm::mat4 to_world;
	unsigned int cascade = 0;

	ShadowParams() {};
};

struct VolumetricPass {
	Handle<struct Texture> depth_prepass;
	FogMap calc_fog;

	Handle<Shader> volume_shader;
	Handle<Shader> upsample_shader;

	VolumetricPass(struct Window&, Handle<struct Texture>);
	void clear();

	void render_with_cascade(struct World&, struct RenderParams&, ShadowParams& shadow_params);
	void render_upsampled(struct World&, Handle<struct Texture>);
};