#pragma once
#include "graphics/device.h"
#include "graphics/frameBuffer.h"
#include "graphics/shadow.h"
#include "graphics/pass.h"

struct MainPass : Pass {
	Device device;
	
	DepthMap depth_prepass;

	ShadowPass shadow_pass;

	Handle<struct Texture> frame_map;
	Framebuffer current_frame;
	
	void render(struct World&, struct RenderParams&) override;
	void set_shader_params(Handle<Shader>, struct World&, struct RenderParams&) override;

	vector<Pass*> post_process;

	MainPass(struct World&, struct Window&);
};
