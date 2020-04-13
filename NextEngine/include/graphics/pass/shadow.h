#pragma once

#include "graphics/pass/pass.h"
#include "graphics/rhi/frame_buffer.h"
#include "graphics/pass/volumetric.h"
#include "core/handle.h"

struct ShaderManager;
struct TextureManager;
struct Assets;
struct ShaderConfig;
struct RenderCtx;
struct Renderer;

struct DepthMap : Pass {
	Assets& assets;

	shader_handle depth_shader;
	texture_handle depth_map;
	Framebuffer depth_map_FBO;

	void set_shader_params(ShaderConfig&, RenderCtx&) override {};

	DepthMap(Assets&, uint, uint, bool stencil = false);
	void render_maps(Renderer& renderer, World&, RenderCtx&, glm::mat4 projection, glm::mat4 view, bool is_shadow_pass = false);
};

struct ShadowMask {
	Assets& assets;

	texture_handle shadow_mask_map;
	Framebuffer shadow_mask_map_fbo;

	void set_shadow_params(ShaderConfig&, RenderCtx&);

	ShadowMask(Assets&, glm::vec2);
};

struct ShadowPass : Pass {
	Renderer& renderer;
	Assets& assets;

	shader_handle shadow_mask_shader;
	shader_handle screenspace_blur_shader;

	texture_handle depth_prepass;
	DepthMap deffered_map_cascade;
	ShadowMask shadow_mask;
	ShadowMask ping_pong_shadow_mask;

	VolumetricPass volumetric;

	void render(World&, RenderCtx&) override;
	void set_shader_params(ShaderConfig&, RenderCtx&) override {};
	void set_shadow_params(ShaderConfig&, RenderCtx&);

	ShadowPass(Assets& assets, Renderer& renderer, glm::vec2, texture_handle depth_prepass);
};
