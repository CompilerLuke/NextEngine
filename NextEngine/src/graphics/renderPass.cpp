#include "stdafx.h"
#include "graphics/renderPass.h"
#include "ecs/ecs.h"
#include "graphics/renderer.h"
#include "graphics/window.h"
#include "graphics/draw.h"
#include "graphics/frameBuffer.h"
#include "components/lights.h"
#include "graphics/ibl.h"
#include "components/transform.h"
#include "components/camera.h"

MainPass::MainPass(World& world, glm::vec2 size)
	: depth_prepass(size.x, size.y, world, true),
	shadow_pass(size, world, depth_prepass.depth_map)
{
	AttachmentSettings attachment(frame_map);
	FramebufferSettings settings;
	settings.width = size.x;
	settings.height = size.y;
	settings.depth_buffer = DepthComponent24;
	settings.stencil_buffer = StencilComponent8;
	settings.color_attachments.append(attachment);

	output.width = size.x;
	output.height = size.y;
	//device.multisampling = 4;
	//device.clear_colour = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	current_frame = Framebuffer(settings);
}

void MainPass::set_shader_params(Handle<Shader> shader, Handle<ShaderConfig> config, World& world, RenderParams& params) {
	if (params.cam) {
		Transform* trans = world.by_id<Transform>(world.id_of(params.cam));
		shader::set_vec3(shader, "viewPos", trans->position, config);
	}

	shadow_pass.set_shadow_params(shader, config, world, params);
	
	if (params.skybox) params.skybox->set_ibl_params(shader, config, world, params);
	if (params.dir_light) {
		shader::set_vec3(shader, "dirLight.direction", params.dir_light->direction, config);
		shader::set_vec3(shader, "dirLight.color", params.dir_light->color, config);
	}
}

#include "graphics/primitives.h"
#include "components/camera.h"

void MainPass::render_to_buffer(World& world, RenderParams& params, std::function<void()> bind) {

	unsigned int width = params.width;
	unsigned int height = params.height;

	//params.width = current_frame.width;
	//params.height = current_frame.height;
	
	params.command_buffer->clear();

	update_camera_matrices(world, get_camera(world, params.layermask), params);

	Renderer::render_view(world, params);

	params.width = current_frame.width;
	params.height = current_frame.height;

	depth_prepass.render_maps(world, params, params.projection, params.view);
	shadow_pass.render(world, params);

	current_frame.bind();
	current_frame.clear_color(glm::vec4(0, 0, 0, 1));
	current_frame.clear_depth(glm::vec4(0, 0, 0, 1));

	//glBlitNamedFramebuffer(depth_prepass.depth_map_FBO.fbo, current_frame.fbo, 0, 0, current_frame.width, current_frame.height, 0, 0, current_frame.width, current_frame.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	params.command_buffer->submit_to_gpu(world, params);

	current_frame.unbind();

	params.width = width;
	params.height = height;

	for (Pass* pass : post_process) {
		pass->render(world, params);
	}

	bind();

	shadow_pass.volumetric.render_upsampled(world, frame_map, params.projection);
}

void MainPass::render(World& world, RenderParams& params) {
	render_to_buffer(world, params, [this, params]() {
		output.width = params.width;
		output.height = params.height;

		output.bind();
		output.clear_color(glm::vec4(0, 0, 0, 1));
		output.clear_depth(glm::vec4(0, 0, 0, 1));
	});
}