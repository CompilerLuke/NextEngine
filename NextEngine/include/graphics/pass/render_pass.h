#pragma once
#include "graphics/rhi/device.h"
#include "graphics/rhi/frame_buffer.h"
#include "graphics/pass/shadow.h"
#include "graphics/pass/pass.h"
#include <functional>

struct AssetManager;
struct RenderCtx;
struct World;
struct ShaderConfig;

struct ENGINE_API MainPass : Pass {
	Framebuffer output;
	
	DepthMap depth_prepass;

	ShadowPass shadow_pass;

	texture_handle frame_map;
	Framebuffer current_frame;
	
	void render_to_buffer(World&, RenderCtx&, std::function<void()>);
	void render(World&, RenderCtx&) override;
	void set_shader_params(ShaderConfig&, RenderCtx&) override;

	vector<Pass*> post_process;

	MainPass(AssetManager& asset_manager, glm::vec2);
	void resize(glm::vec2);
};