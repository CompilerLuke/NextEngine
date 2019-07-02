#include "stdafx.h"
#include "graphics/renderPass.h"
#include "ecs/ecs.h"
#include "graphics/window.h"
#include "graphics/draw.h"
#include "graphics/frameBuffer.h"
#include "components/lights.h"
#include "graphics/ibl.h"

MainPass::MainPass(World& world, Window& window)
	: depth_prepass(window.width, window.height, world),
	shadow_pass(window, world, depth_prepass.depth_map)
{
	AttachmentSettings attachment(frame_map);
	FramebufferSettings settings;
	settings.width = window.width;
	settings.height = window.height;
	settings.depth_buffer = DepthComponent24;
	settings.stencil_buffer = StencilComponent8;
	settings.color_attachments.append(attachment);

	device.width = window.width;
	device.height = window.height;
	device.multisampling = 4;
	device.clear_colour = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	current_frame = Framebuffer(settings);
}

void MainPass::set_shader_params(Handle<Shader> shader, World& world, RenderParams& params) {
	shader::set_mat4(shader, "projection", params.projection);
	shader::set_mat4(shader, "view", params.view);

	shadow_pass.set_shadow_params(shader, world, params);
	
	if (params.skybox) params.skybox->set_ibl_params(shader, world, params);
	if (params.dir_light) {
		shader::set_vec3(shader, "dirLight.direction", params.dir_light->direction);
		shader::set_vec3(shader, "dirLight.color", params.dir_light->color);
	}
}

void MainPass::render(World& world, RenderParams& params) {
	device.width = params.width;
	device.height = params.height;

	params.command_buffer->clear();

	world.render(params);

	depth_prepass.render_maps(world, params, params.projection, params.view);
	shadow_pass.render(world, params);

	current_frame.bind();
	current_frame.clear_color(glm::vec4(0, 0, 0, 1));
	current_frame.clear_depth(glm::vec4(0, 0, 0, 1));

	params.command_buffer->submit_to_gpu(world, params);

	current_frame.unbind();

	for (Pass* pass : post_process) {
		pass->render(world, params);
	}

	device.bind();
	shadow_pass.volumetric.render_upsampled(world, frame_map);
}