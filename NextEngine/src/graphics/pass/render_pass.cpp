#include "graphics/pass/render_pass.h"
#include "ecs/ecs.h"
#include "graphics/assets/assets.h"
#include "graphics/renderer/renderer.h"
#include "graphics/rhi/draw.h"
#include "graphics/rhi/frame_buffer.h"
#include "components/lights.h"
#include "graphics/renderer/ibl.h"
#include "components/transform.h"
#include "components/camera.h"

void fill_pass_ubo(PassUBO& pass_ubo, const Viewport& viewport) {
	pass_ubo.proj = viewport.proj;
	pass_ubo.view = viewport.view;
	pass_ubo.resolution.x = viewport.width;
	pass_ubo.resolution.y = viewport.height;

#ifdef RENDER_API_VULKAN
	pass_ubo.proj[1][1] *= -1;
#endif
}

MainPass::MainPass(Renderer& renderer, glm::vec2 size)
	:
	renderer(renderer),
	depth_prepass(size.x, size.y, true),
	shadow_pass(renderer, size * 0.25f, depth_prepass.depth_map)
{
	FramebufferDesc desc{ size.x, size.y };
	desc.depth_buffer = DepthComponent24;
	desc.stencil_buffer = StencilComponent8;
	
	add_color_attachment(desc, &frame_map);
	
	output.width = size.x;
	output.height = size.y;
	//device.multisampling = 4;
	//device.clear_colour = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	make_Framebuffer(RenderPass::Scene, desc);
}

 struct LightingUBO {
	alignas(16) glm::vec3 view_pos;
	alignas(16) glm::vec3 dir_light_direction;
	alignas(16) glm::vec3 dir_light_color;
};

//todo this should be a uniform buffer, this current solution is terrible!!
void MainPass::set_shader_params(ShaderConfig& config, RenderPass& ctx) {
	/*if (ctx.cam) {
		config.set_vec3("viewPos", ctx.view_pos);
	}

	shadow_pass.set_shadow_params(config, ctx);

	if (ctx.skybox) renderer.skybox_renderer->bind_ibl_params(config, ctx);
	if (ctx.dir_light) {
		config.set_vec3("dirLight.direction", ctx.dir_light->direction);
		config.set_vec3("dirLight.color", ctx.dir_light->color);
	}*/
}

#include "graphics/rhi/primitives.h"
#include "components/camera.h"

/*
void MainPass::render_to_buffer(World& world, RenderPass& ctx, std::function<void()> bind) {

	unsigned int width = ctx.width;
	unsigned int height = ctx.height;

	//params.width = current_frame.width;
	//params.height = current_frame.height;
	
	//update_camera_matrices(world, get_camera(world, ctx.layermask), ctx);

	renderer.render_view(world, ctx);
	renderer.render_overlay(world, ctx);

	ctx.width = current_frame.width;
	ctx.height = current_frame.height;

	depth_prepass.render_maps(renderer, world, ctx, ctx.projection, ctx.view);

	shadow_pass.render(world, ctx);

	current_frame.bind();
	current_frame.clear_color(glm::vec4(0, 0, 0, 1));
	current_frame.clear_depth(glm::vec4(0, 0, 0, 1));

	//glBlitNamedFramebuffer(depth_prepass.depth_map_FBO.fbo, current_frame.fbo, 0, 0, 0, 0, current_frame.width, current_frame.height, current_frame.width, current_frame.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	current_frame.bind();
	current_frame.clear_color(glm::vec4(0, 0, 0, 1));
	current_frame.clear_depth(glm::vec4(0, 0, 0, 1));
	
	//glClear(GL_STENCIL_BUFFER_BIT);

	ctx.command_buffer.submit();
	current_frame.unbind();

	ctx.width = width;
	ctx.height = height;

	for (RenderPass* pass : post_process) {
		pass->render(world, ctx);
	}

	bind();

	shadow_pass.volumetric.render_upsampled(world, frame_map, ctx.projection);
}

void MainPass::render(World& world, RenderPass& params) {
	render_to_buffer(world, params, [this, params]() {
		output.width = params.width;
		output.height = params.height;

		output.bind();
		output.clear_color(glm::vec4(0, 0, 0, 1));
		output.clear_depth(glm::vec4(0, 0, 0, 1));
	});
}

*/

