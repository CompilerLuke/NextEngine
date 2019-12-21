#include "stdafx.h"
#include "picking.h"
#include <glad/glad.h>
#include "editor.h"
#include "core/input.h"
#include "graphics/draw.h"
#include "ecs/layermask.h"
#include "core/temporary.h"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "graphics/shader.h"
#include "core/temporary.h"
#include "graphics/window.h"
#include "logger/logger.h"
#include "graphics/rhi.h"

PickingPass::PickingPass(Window& params, MainPass* main_pass)
: picking_shader(load_Shader("shaders/picking.vert", "shaders/picking.frag")) {
	
	AttachmentSettings color_attachment(picking_map);
	color_attachment.internal_format = R32I;
	color_attachment.external_format = Red_Int;
	color_attachment.texel_type = Int_Texel;
	color_attachment.min_filter = Nearest;
	color_attachment.mag_filter = Nearest;

	FramebufferSettings settings;
	settings.color_attachments.append(color_attachment);
	settings.width = params.width;
	settings.height = params.height;

	framebuffer = Framebuffer(settings);

	this->main_pass = main_pass;
}

void PickingPass::set_shader_params(Handle<Shader> shader, Handle<ShaderConfig> config, World& world, RenderParams& params) {

}

glm::vec2 PickingPass::picking_location(Input& input) {
	auto mouse_position = input.mouse_position;
	return mouse_position / (input.region_max - input.region_min) * glm::vec2(framebuffer.width, framebuffer.height);
}

int PickingPass::pick(World& world, Input& input) {
	int id = 0;

	glm::vec2 pick_at = picking_location(input);

	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer.fbo);
	glReadPixels((int)pick_at.x, (int)this->framebuffer.height - pick_at.y, 1, 1, GL_RED_INTEGER, GL_INT, &id);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	id -= 1;

	return id;
}

float PickingPass::pick_depth(World& world, Input& input) { //todo pick from frame buffer
	float depth = 0;

	glm::vec2 pick_at = picking_location(input);

	glBindFramebuffer(GL_FRAMEBUFFER, this->main_pass->depth_prepass.depth_map_FBO.fbo);
	glReadPixels((int)pick_at.x, (int)this->framebuffer.height - pick_at.y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	log("got back ", depth);

	return depth;
}

void PickingPass::render(World& world, RenderParams& params) {
	if (!(params.layermask & editor_layer)) return;

	CommandBuffer cmd_buffer;
	RenderParams new_params = params;
	new_params.command_buffer = &cmd_buffer;
	new_params.width = framebuffer.width;
	new_params.height = framebuffer.height;
	new_params.projection = params.projection;
	new_params.view = params.view;
	new_params.layermask = picking_layer;
	new_params.pass = this;

	Renderer::render_view(world, params);

	Handle<Uniform> loc = location(picking_shader, "id");

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

	cmd_buffer.submit_to_gpu(world, params);

	framebuffer.unbind();
}

PickingSystem::PickingSystem(Editor& editor) : editor(editor) {
	this->outline_shader = load_Shader("shaders/outline.vert", "shaders/outline.frag"); //todo move into constructor

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

void PickingSystem::render(World& world, RenderParams& params) {
	if (!(params.layermask & editor_layer)) return;
	if (params.layermask & picking_layer) return;

	//Render Outline
	int selected = editor.selected_id;
	if (selected == -1) return;

	for (DrawCommand& cmd : params.command_buffer->commands) {
		if (cmd.id != selected) continue;
		if (cmd.num_instances > 1) continue;

		{
			Shader* shad = RHI::shader_manager.get(cmd.material->shader);
			Material* mat = TEMPORARY_ALLOC(Material);
			mat->params.allocator = &temporary_allocator;
			*mat = *cmd.material;

			mat->state = &object_state;
			cmd.material = mat;
			
		}

		{

			   
			DrawCommand new_cmd = cmd;
			new_cmd.material = TEMPORARY_ALLOC(Material, outline_material); //todo change vertex shader
			params.command_buffer->submit(new_cmd);
		}
	}

	glLineWidth(5.0);


}

