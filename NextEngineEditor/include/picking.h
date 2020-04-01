#pragma once

#include "core/handle.h"
#include "graphics/renderer/renderer.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/renderer/material_system.h"
#include "graphics/pass/pass.h"
#include "graphics/rhi/frame_buffer.h"

struct World;
struct RenderCtx;
struct ShaderConfig;
struct Input;

struct PickingPass : Pass {
	Renderer& renderer;
	AssetManager& asset_manager;

	shader_handle picking_shader;
	Framebuffer framebuffer;
	texture_handle picking_map;
	struct MainPass* main_pass;
	
	void set_shader_params(ShaderConfig&, RenderCtx&) override;

	int pick(World&, Input&);
	float pick_depth(World&, Input&);

	void render(World&, RenderCtx&) override;

	glm::vec2 picking_location(Input&);

	PickingPass(AssetManager&, glm::vec2, MainPass*);
};

struct PickingSystem : RenderFeature {
	struct Editor& editor;
	shader_handle outline_shader;
	Material outline_material;
	DrawCommandState object_state;
	DrawCommandState outline_state;

	PickingSystem(Editor&, ShaderManager&);

	void render(struct World&, struct RenderCtx&) override;
};