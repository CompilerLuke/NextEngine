#pragma once

#include "core/handle.h"
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>

struct World;
struct RenderPass;
struct CommandBuffer;
struct Framebuffer;

struct Viewport {
	uint width, height;
	uint x, y;

	glm::mat4 proj;
	glm::mat4 view;
	glm::mat4 proj_view;

	glm::vec3 cam_pos;
};

struct PassUBO {
	glm::vec4 resolution;
	glm::mat4 proj;
	glm::mat4 view;
};

struct ENGINE_API RenderPass {
	enum ID { Scene, Shadow0, Shadow1, Shadow2, Shadow3, ScenePassCount, Screen = ScenePassCount, Volumetric, IBLCapture, TerrainHeightGeneration, TerrainTextureGeneration, PassCount };
	enum Type { Color, Depth };

	ID id = Scene;
	Type type = Color;

	render_pass_handle render_pass;
	Viewport viewport;

	CommandBuffer& cmd_buffer;
};

ENGINE_API void fill_pass_ubo(PassUBO& pass_ubo, const Viewport& viewport);

ENGINE_API RenderPass begin_render_pass(RenderPass::ID id, glm::vec4 clear_color = glm::vec4(0.1, 0.1, 0.1, 1.0)); // RenderPass::PassType type, Framebuffer&);
//void bind_pass_ubo(RenderPass&, );
ENGINE_API void end_render_pass(RenderPass&);

ENGINE_API void next_subpass(RenderPass&);

ENGINE_API RenderPass begin_render_frame();
ENGINE_API void end_render_frame(RenderPass&);

ENGINE_API RenderPass::Type render_pass_type_by_id(RenderPass::ID id, uint subpass);
ENGINE_API render_pass_handle render_pass_by_id(RenderPass::ID id);
ENGINE_API Viewport render_pass_viewport_by_id(RenderPass::ID id);