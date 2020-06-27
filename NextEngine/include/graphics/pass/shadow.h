#pragma once

#include "graphics/pass/pass.h"
#include "graphics/rhi/rhi.h"
#include "graphics/pass/volumetric.h"
#include "core/handle.h"

struct ShaderManager;
struct TextureManager;
struct Assets;
struct ShaderConfig;
struct RenderPass;
struct Renderer;

struct DepthMap {
	shader_handle depth_shader;
	texture_handle depth_map;
	Framebuffer depth_map_FBO;

	DepthMap(uint, uint, bool stencil = false);
	void render_maps(Renderer& renderer, World&, RenderPass&, glm::mat4 projection, glm::mat4 view, bool is_shadow_pass = false);
};

struct ShadowMask {
	texture_handle shadow_mask_map;
	Framebuffer shadow_mask_map_fbo;

	void set_shadow_params(ShaderConfig&, RenderPass&);

	ShadowMask(glm::vec2);
};

struct ShadowPass {
	Renderer& renderer;

	shader_handle shadow_mask_shader;
	shader_handle screenspace_blur_shader;

	texture_handle depth_prepass;
	DepthMap deffered_map_cascade;
	ShadowMask shadow_mask;
	ShadowMask ping_pong_shadow_mask;

	VolumetricPass volumetric;

	void render(World&, RenderPass&);
	void set_shadow_params(ShaderConfig&, RenderPass&);

	ShadowPass(Renderer& renderer, glm::vec2, texture_handle depth_prepass);
};
