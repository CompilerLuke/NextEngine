#include "stdafx.h"
#include "picking.h"
#include <glad/glad.h>
#include "editor.h"
#include "core/io/input.h"
#include "graphics/rhi/draw.h"
#include "ecs/layermask.h"
#include "core/memory/linear_allocator.h"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "graphics/assets/shader.h"
#include "core/memory/linear_allocator.h"
#include "core/io/logger.h"
#include "graphics/assets/asset_manager.h"
#include "engine/engine.h"

PickingPass::PickingPass(AssetManager& asset_manager, glm::vec2 size, MainPass* main_pass)
: asset_manager(asset_manager), picking_shader(asset_manager.shaders.load("shaders/picking.vert", "shaders/picking.frag")), renderer(main_pass->renderer) {
	
	AttachmentDesc color_attachment(picking_map);
	color_attachment.internal_format = InternalColorFormat::R32I;
	color_attachment.external_format = ColorFormat::Red_Int;
	color_attachment.texel_type = TexelType::Int;
	color_attachment.min_filter = Filter::Nearest;
	color_attachment.mag_filter = Filter::Nearest;

	FramebufferDesc settings;
	settings.color_attachments.append(color_attachment);
	settings.width = size.x;
	settings.height = size.y;

	framebuffer = Framebuffer(asset_manager.textures, settings);

	this->main_pass = main_pass;
}

void PickingPass::set_shader_params(ShaderConfig& config, RenderCtx& params) {

}

glm::vec2 PickingPass::picking_location(Input& input) {
	auto mouse_position = input.mouse_position;
	return mouse_position / (input.region_max - input.region_min) * glm::vec2(framebuffer.width, framebuffer.height);
}

int PickingPass::pick(World& world, Input& input) {
	int id = 0;

	glm::vec2 pick_at = picking_location(input);

	framebuffer.read_pixels(pick_at.x, framebuffer.height - pick_at.y, 1, 1, GL_RED, GL_INT, &id);

	id -= 1;

	return id;
}

float PickingPass::pick_depth(World& world, Input& input) { //todo pick from frame buffer
	float depth = 0;

	glm::vec2 pick_at = picking_location(input);

	framebuffer.read_pixels(pick_at.x, framebuffer.height - pick_at.y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

	log("got back ", depth);

	return depth;
}

void PickingPass::render(World& world, RenderCtx& ctx) {
	return;
	/*if (!(ctx.layermask & EDITOR_LAYER)) return;

	CommandBuffer cmd_buffer(asset_manager);
	RenderCtx new_ctx(ctx, cmd_buffer, this);
	new_ctx.width = framebuffer.width;
	new_ctx.height = framebuffer.height;
	new_ctx.layermask = PICKING_LAYER;

	renderer.render_view(world, ctx);

	uniform_handle loc = asset_manager.location(picking_shader, "id");

	for (DrawCommand& cmd : cmd_buffer.commands) {
		if (cmd.num_instances > 1) continue;

		DrawCommandState* state = cmd.material->state;

		cmd.material = TEMPORARY_ALLOC(Material, picking_shader);
		cmd.material->state = state;
		cmd.material->set_int("id", cmd.id);
	}

	framebuffer.bind();
	framebuffer.clear_color(glm::vec4(0.0, 0.0, 0.0, 1.0));
	framebuffer.clear_depth(glm::vec4(0.0, 0.0, 0.0, 1.0));

	CommandBuffer::submit_to_gpu(ctx);

	framebuffer.unbind();*/
}

PickingSystem::PickingSystem(Editor& editor, ShaderManager& shader_manager) : editor(editor) {
	this->outline_shader = shader_manager.load("shaders/outline.vert", "shaders/outline.frag"); //todo move into constructor

	outline_state.order = (DrawOrder)4;
	outline_state.mode = DrawWireframe;
	outline_state.stencil_func = StencilFunc_NotEqual;
	outline_state.stencil_op = Stencil_Keep_Replace;
	outline_state.stencil_mask = 0x00;
	object_state.clear_depth_buffer = false;

	object_state.order = (DrawOrder)3;
	object_state.stencil_func = StencilFunc_Always;
	object_state.stencil_mask = 0xFF;
	object_state.stencil_op = Stencil_Keep_Replace;
	object_state.clear_stencil_buffer = true;
	object_state.clear_depth_buffer = false;

	this->outline_material = Material(outline_shader);
	this->outline_material.state = &outline_state;
}

void PickingSystem::render(World& world, RenderCtx& params) { //THIS NO LONGER WORKS!!!
	/*
	if (!(params.layermask & EDITOR_LAYER)) return;
	if (params.layermask & PICKING_LAYER) return;

	//Render Outline
	int selected = editor.selected_id;
	if (selected == -1) return;

	for (DrawCommand& cmd : params.command_buffer.commands) {
		if (cmd.num_instances > 1) continue;

		{
			//Shader* shad = RHI::shader_manager.get(cmd.material->shader);
			Material* mat = TEMPORARY_ALLOC(Material);
			mat->params.allocator = &temporary_allocator;

			*mat = *cmd.material;

			mat->state = &object_state;
			cmd.material = mat;
			
		}

		{
			DrawCommand new_cmd = cmd;
			new_cmd.material = TEMPORARY_ALLOC(Material, outline_material); //todo change vertex shader
			params.command_buffer;
		}
	}

	glLineWidth(5.0);
	*/
}

