#pragma once
#include "graphics/rhi/rhi.h"
#include "graphics/pass/shadow.h"
#include "graphics/pass/pass.h"
#include <functional>

struct Assets;
struct RenderPass;
struct World;
struct ShaderConfig;

struct MainPass {
	Renderer& renderer;
	Framebuffer output;
	
	DepthMap depth_prepass;

	ShadowPass shadow_pass;

	texture_handle frame_map;
	Framebuffer current_frame;
	
	void ENGINE_API render_to_buffer(World&, RenderPass&, std::function<void()>);
	void ENGINE_API render(World&, RenderPass&);
	void ENGINE_API set_shader_params(ShaderConfig&, RenderPass&);

	vector<RenderPass*> post_process;

	ENGINE_API MainPass(Renderer& renderer, glm::vec2);
	void ENGINE_API resize(glm::vec2);
};