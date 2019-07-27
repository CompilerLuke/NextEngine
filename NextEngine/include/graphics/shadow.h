#pragma once

#include "pass.h"
#include "frameBuffer.h"
#include "volumetric.h"
#include "core/handle.h"

struct DepthMap : Pass {
	Handle<Shader> depth_shader;

	Handle<struct Texture> depth_map;
	Framebuffer depth_map_FBO;

	void set_shader_params(Handle<struct Shader>, Handle<struct ShaderConfig>, struct World&, struct RenderParams&) override {};

	DepthMap(unsigned int, unsigned int, struct World&);
	void render_maps(struct World&, struct RenderParams&, glm::mat4 projection, glm::mat4 view);
};

struct ShadowMask {
	Handle<struct Texture> shadow_mask_map;
	Framebuffer shadow_mask_map_fbo;

	void set_shadow_params(Handle<Shader>, Handle<ShaderConfig>, struct World&, struct RenderParams&);

	ShadowMask(struct Window& window, struct World&);
};

struct ShadowPass : Pass {
	Handle<Shader> shadow_mask_shader;
	Handle<Shader> screenspace_blur_shader;

	Handle<struct Texture> depth_prepass;
	DepthMap deffered_map_cascade;
	ShadowMask shadow_mask;
	ShadowMask ping_pong_shadow_mask;

	VolumetricPass volumetric;

	void render(struct World&, struct RenderParams&) override;
	void set_shader_params(Handle<Shader>, Handle<ShaderConfig>, struct World&, struct RenderParams&) override {};
	void set_shadow_params(Handle<Shader>, Handle<ShaderConfig>, struct World&, struct RenderParams&);

	ShadowPass(struct Window& window, struct World&, Handle<struct Texture> depth_prepass);
};
