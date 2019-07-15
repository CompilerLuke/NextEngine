#include "stdafx.h"
#include "editor/picking.h"
#include <glad/glad.h>
#include "editor/editor.h"
#include "core/input.h"
#include "graphics/draw.h"
#include "ecs/layermask.h"
#include "core/temporary.h"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "graphics/shader.h"
#include "core/temporary.h"
#include "editor/editor.h"
#include "graphics/window.h"
#include "logger/logger.h"
#include "graphics/rhi.h"

PickingPass::PickingPass(Window& params)
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
}

void PickingPass::set_shader_params(Handle<Shader> shader, World& world, RenderParams& params) {

}

int PickingPass::pick(World& world, UpdateParams& params) {
	auto mouse_position = params.input.mouse_position;

	int id = 0;

	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer.fbo);
	glReadPixels((int)mouse_position.x, (int)this->framebuffer.height - mouse_position.y, 1, 1, GL_RED_INTEGER, GL_INT, &id);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	id -= 1;

	log("Got back id : ", id);

	return id;
}

void PickingPass::render(World& world, RenderParams& params) {
	if (!params.layermask & editor_layer) return;

	CommandBuffer cmd_buffer;
	RenderParams new_params(&cmd_buffer, this);
	new_params.width = framebuffer.width;
	new_params.height = framebuffer.height;
	new_params.projection = params.projection;
	new_params.view = params.view;
	new_params.layermask = picking_layer;
	new_params.pass = this;

	world.render(new_params);

	Handle<Uniform> loc = location(picking_shader, "id");

	for (DrawCommand& cmd : cmd_buffer.commands) {
		vector<Param> params;
		params.allocator = &temporary_allocator;
		params.append(make_Param_Int(loc, cmd.id));

		cmd.material = TEMPORARY_ALLOC(Material, {
			"Picking Material", //todo this will allocate
			picking_shader,
			params,
			cmd.material->state
		});
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

	object_state.order = (DrawOrder)3;
	object_state.stencil_func = StencilFunc_Always;
	object_state.stencil_mask = 0xFF;
	object_state.stencil_op = Stencil_Keep_Replace;
	object_state.clear_stencil_buffer = true;

	this->outline_material = {
		"OutlineMaterial",
		outline_shader,
		{},
		&outline_state
	};
}

void PickingSystem::render(World& world, RenderParams& params) {
	if (!params.layermask & editor_layer) return;
	if (params.layermask & picking_layer) return;

	//Render Outline
	int selected = editor.selected_id;
	if (selected == -1) return;

	for (DrawCommand& cmd : params.command_buffer->commands) {
		if (cmd.id != selected) continue;

		{
			Shader* shad = RHI::shader_manager.get(cmd.material->shader);
			Material* mat = TEMPORARY_ALLOC(Material, *cmd.material); //todo LEAKS MEMORY
			mat->state = &object_state;
			cmd.material = mat;
			
		}

		{
			DrawCommand new_cmd = cmd;
			new_cmd.material = TEMPORARY_ALLOC(Material, outline_material);
			params.command_buffer->submit(new_cmd);
		}
	}

	glLineWidth(5.0);
}

