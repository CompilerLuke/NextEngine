#pragma once

#include "stdafx.h"

#include "graphics/rhi.h"
#include "engine/engine.h"
#include "graphics/renderer.h"
#include "graphics/terrain.h"
#include "graphics/grass.h"
#include "graphics/modelRendering.h"
#include "graphics/transforms.h"
#include "graphics/shader.h"
#include "graphics/draw.h"
#include "graphics/renderPass.h"

PreRenderParams::PreRenderParams(Renderer& renderer, Layermask mask) : renderer(renderer), layermask(mask) {}

RenderParams::RenderParams(Renderer& renderer, CommandBuffer* command_buffer, Pass* pass) :
	command_buffer(command_buffer),
	cam(NULL),
	pass(pass),
	renderer(renderer) {}

RenderParams::RenderParams(Engine& engine, Layermask mask, RenderExtension* ext) :
	renderer(engine.renderer),
	layermask(mask),
	extension(ext) {}

void Renderer::set_shader_scene_params(World& world, RenderParams& params, Handle<Shader> shader, Handle<ShaderConfig> config) {

	shader::set_mat4(shader, "projection", params.projection, config);

	shader::set_mat4(shader, "projection", params.projection, config);
	shader::set_mat4(shader, "view", params.view, config);

	shader::set_float(shader, "window_width", params.width, config);
	shader::set_float(shader, "window_height", params.height, config);

	params.pass->set_shader_params(shader, config, world, params);
}

void RenderParams::set_shader_scene_params(Handle<Shader> shader, Handle<ShaderConfig> config, World& world) {
	Renderer::set_shader_scene_params(world, *this, shader, config);
}



void Renderer::update_settings(const RenderSettings& settings) {
	this->settings = settings;
}

void Renderer::set_render_pass(Pass* pass) {
	this->main_pass = std::unique_ptr<Pass>(pass);
}

void Renderer::render_view(World& world, RenderParams& params) {
	Renderer& renderer = params.renderer;
	renderer.model_renderer->render(world, params);
	renderer.terrain_renderer->render(world, params);
	renderer.grass_renderer->render(world, params);
}

void Renderer::render(World& world, RenderParams& params) {	
	Renderer& renderer = params.renderer;

	PreRenderParams pre_render(renderer, params.layermask);

	compute_model_matrices(renderer.model_m, world, params.layermask);
	renderer.model_renderer->pre_render(world, pre_render);

	if (params.extension == NULL) {
		renderer.main_pass.get()->render(world, params);
	}
	else {
		params.extension->render(world, params);
	}
}