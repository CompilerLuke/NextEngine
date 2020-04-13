#pragma once
#include "graphics/rhi/device.h"
#include "graphics/rhi/frame_buffer.h"
#include "graphics/pass/shadow.h"
#include "graphics/pass/pass.h"
#include <functional>

struct Assets;
struct RenderCtx;
struct World;
struct ShaderConfig;

struct MainPass : Pass {
	Renderer& renderer;
	Framebuffer output;
	
	DepthMap depth_prepass;

	ShadowPass shadow_pass;

	texture_handle frame_map;
	Framebuffer current_frame;
	
	void ENGINE_API render_to_buffer(World&, RenderCtx&, std::function<void()>);
	void ENGINE_API render(World&, RenderCtx&) override;
	void ENGINE_API set_shader_params(ShaderConfig&, RenderCtx&) override;

	vector<Pass*> post_process;

	ENGINE_API MainPass(Renderer& renderer,Assets& assets, glm::vec2);
	void ENGINE_API resize(glm::vec2);
};