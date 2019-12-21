#pragma once
#include "graphics/device.h"
#include "graphics/frameBuffer.h"
#include "graphics/shadow.h"
#include "graphics/pass.h"
#include <functional>

struct ENGINE_API MainPass : Pass {
	Framebuffer output;
	
	DepthMap depth_prepass;

	ShadowPass shadow_pass;

	Handle<struct Texture> frame_map;
	Framebuffer current_frame;
	
	void render_to_buffer(struct World&, struct RenderParams&, std::function<void()>);
	void render(struct World&, struct RenderParams&) override;
	void set_shader_params(Handle<struct Shader>, Handle<struct ShaderConfig>, struct World&, struct RenderParams&) override;

	vector<Pass*> post_process;

	MainPass(struct World&, glm::vec2);

	void resize(glm::vec2);
};