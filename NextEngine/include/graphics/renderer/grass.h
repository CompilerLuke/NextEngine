#pragma once

#include "engine/handle.h"
#include "core/container/tvector.h"
#include "graphics/rhi/buffer.h"
#include <glm/mat4x4.hpp>
#include "graphics/pass/pass.h"

/*
struct GrassRenderSystem : RenderFeature {
	GrassRenderSystem(World& world);
	void render(struct World&, struct RenderPass&) override;

	static void ENGINE_API update_buffers(World& world, ID id);
};
*/

struct World;
struct RenderPass;
struct Viewport;

struct GrassInstance {
	VertexBuffer vertex_buffer;
	slice<glm::mat4> instances;
	pipeline_handle depth_pipeline;
	pipeline_handle depth_prepass_pipeline;
	pipeline_handle color_pipeline;
	material_handle material;
};

struct GrassRenderData {
	tvector<GrassInstance> instances[RenderPass::ScenePassCount];
};

void extract_grass_render_data(GrassRenderData&, World&, Viewport planes[]);
void render_grass(const GrassRenderData&, RenderPass& render_pass);
