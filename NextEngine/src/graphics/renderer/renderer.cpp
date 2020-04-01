#pragma once

#include "stdafx.h"

#include "engine/engine.h"
#include "graphics/renderer/renderer.h"
#include "graphics/renderer/terrain.h"
#include "graphics/renderer/grass.h"
#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/transforms.h"
#include "graphics/assets/shader.h"
#include "components/camera.h"
#include "graphics/rhi/draw.h"
#include "graphics/pass/render_pass.h"
#include "graphics/renderer/ibl.h"
#include "components/lights.h"
#include "core/memory/linear_allocator.h"
#include "graphics/pass/pass.h"
#include "graphics/rhi/window.h"

Renderer::Renderer() {}

void Renderer::init(AssetManager& asset_manager, Window& window, World& world) {
	model_renderer    = PERMANENT_ALLOC(ModelRendererSystem, asset_manager);
	grass_renderer    = PERMANENT_ALLOC(GrassRenderSystem, world);
	terrain_renderer  = PERMANENT_ALLOC(TerrainRenderSystem, asset_manager, world);
	skybox_renderer   = PERMANENT_ALLOC(SkyboxSystem, asset_manager, world);
	main_pass         = PERMANENT_ALLOC(MainPass, *this, asset_manager, glm::vec2(window.width, window.height));
	culling			  = PERMANENT_ALLOC(CullingSystem, asset_manager, *model_renderer, world);
}

Renderer::~Renderer() {
	destruct(model_renderer);
	destruct(grass_renderer); 
	destruct(terrain_renderer); 
	destruct(skybox_renderer);
	destruct(main_pass);
}

PreRenderParams::PreRenderParams(Layermask mask) : layermask(mask) {}

RenderCtx::RenderCtx(CommandBuffer& command_buffer, Pass* pass) :
	command_buffer(command_buffer),
	cam(NULL),
	pass(pass)
{}

RenderCtx::RenderCtx(const RenderCtx& ctx, CommandBuffer& command_buffer, Pass* pass) :
	layermask(ctx.layermask),
	extension(ctx.extension),
	skybox(ctx.skybox),
	dir_light(ctx.dir_light),
	view_pos(ctx.view_pos),
	projection(ctx.projection),
	view(ctx.view),
	model_m(ctx.model_m),
	width(ctx.width),
	height(ctx.height),
	command_buffer(command_buffer),
	pass(pass)
{

}

void RenderCtx::set_shader_scene_params(ShaderConfig& config) {
	config.set_mat4("projection", projection);
	config.set_mat4("view", view);
	config.set_float("window_width", width);
	config.set_float("window_height", height);

	pass->set_shader_params(config, *this);
}

void Renderer::update_settings(const RenderSettings& settings) {
	this->settings = settings;
}

//void Renderer::set_render_pass(Pass* pass) {
//	this->main_pass = std::unique_ptr<Pass>(pass);
//}

void Renderer::render_view(World& world, RenderCtx& ctx) {
	culling->cull(world, ctx);
	model_renderer->render(world, ctx);
	terrain_renderer->render(world, ctx);
	grass_renderer->render(world, ctx);
	skybox_renderer->render(world, ctx);
}

void Renderer::render_overlay(World& world, RenderCtx& ctx) {
	culling->render_debug_bvh(world, ctx);
}

RenderCtx Renderer::render(World& world, Layermask layermask, uint width, uint height, RenderExtension* ext) {	
	model_m = TEMPORARY_ARRAY(glm::mat4, 1000); //todo find real number

	PreRenderParams pre_render(layermask);

	CommandBuffer cmd_buffer(model_renderer->asset_manager);
	
	RenderCtx ctx(cmd_buffer, main_pass);
	ctx.layermask = layermask;
	ctx.pass = main_pass;
	ctx.skybox = world.filter<Skybox>(ctx.layermask)[0];
	ctx.dir_light = world.filter<DirLight>(ctx.layermask)[0];
	ctx.extension = ext;

	ID camera_id = get_camera(world, ctx.layermask & EDITOR_LAYER ? EDITOR_LAYER : GAME_LAYER);
	update_camera_matrices(world, camera_id, ctx);

	compute_model_matrices(model_m, world, ctx.layermask);
	model_renderer->pre_render();

	if (ctx.extension == NULL) {
		main_pass->render(world, ctx);
	}
	else {
		ctx.extension->render(world, ctx);
	}

	return ctx;
}