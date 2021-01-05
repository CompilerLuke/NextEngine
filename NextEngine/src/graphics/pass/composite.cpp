#include "graphics/pass/composite.h"
#include "graphics/rhi/shader_access.h"
#include "graphics/rhi/pipeline.h"
#include "graphics/rhi/frame_buffer.h"
#include "graphics/rhi/draw.h"
#include "graphics/rhi/primitives.h"
#include "graphics/assets/assets.h"

ENGINE_API uint get_frame_index(); //todo move into header

void make_composite_resources(CompositeResources& resources, texture_handle depth, texture_handle scene, texture_handle volumetric, texture_handle cloud, uint width, uint height) {
	sampler_handle sampler = query_Sampler({});

	for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		resources.ubo[i] = alloc_ubo_buffer(sizeof(CompositeUBO), UBO_PERMANENT_MAP);

		DescriptorDesc descriptor_desc = {};
		add_ubo(descriptor_desc, FRAGMENT_STAGE, resources.ubo[i], 0);
		add_combined_sampler(descriptor_desc, FRAGMENT_STAGE, sampler, depth, 1);
		add_combined_sampler(descriptor_desc, FRAGMENT_STAGE, sampler, scene, 2);
		add_combined_sampler(descriptor_desc, FRAGMENT_STAGE, sampler, volumetric, 3);
		add_combined_sampler(descriptor_desc, FRAGMENT_STAGE, sampler, cloud, 4);

		update_descriptor_set(resources.descriptor[i], descriptor_desc);
	}

	FramebufferDesc framebuffer_desc{width, height};
	add_color_attachment(framebuffer_desc, &resources.composite_map);
	add_dependency(framebuffer_desc, FRAGMENT_STAGE, RenderPass::Scene);
	add_dependency(framebuffer_desc, FRAGMENT_STAGE, RenderPass::Volumetric);

	make_Framebuffer(RenderPass::Composite, framebuffer_desc);

	GraphicsPipelineDesc pipeline_desc = {};
	pipeline_desc.shader = load_Shader("shaders/screenspace.vert", "shaders/composite.frag");
	pipeline_desc.render_pass = RenderPass::Composite;

	resources.pipeline = query_Pipeline(pipeline_desc);
}

void fill_composite_ubo(CompositeUBO& ubo, Viewport& viewport) {
	ubo.depth_proj = glm::inverse(viewport.proj);
}

void render_composite_pass(CompositeResources& resources) {
	RenderPass render_pass = begin_render_pass(RenderPass::Composite);
	CommandBuffer& cmd_buffer = render_pass.cmd_buffer;
	bind_pipeline(cmd_buffer, resources.pipeline);
	bind_descriptor(cmd_buffer, 1, resources.descriptor[get_frame_index()]);
	draw_quad(cmd_buffer);
	end_render_pass(render_pass);
}
