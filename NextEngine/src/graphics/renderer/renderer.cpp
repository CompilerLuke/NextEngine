#pragma once

#include "engine/engine.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/assets.h"
#include "components/camera.h"
#include "graphics/rhi/draw.h"
#include "core/memory/linear_allocator.h"
#include "graphics/pass/pass.h"
#include "graphics/rhi/window.h"
#include "graphics/rhi/vulkan/buffer.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/culling/culling.h"
#include "core/profiler.h"
#include "core/time.h"

//HACK ACKWARD INITIALIZATION
#include "ecs/ecs.h"

uint get_frame_index() {
	return rhi.frame_index;
}

Renderer* make_Renderer(const RenderSettings& settings, World& world) {
	Renderer* renderer = PERMANENT_ALLOC(Renderer);
	renderer->settings = settings;

	ID skybox = make_default_Skybox(world, "engine/Tropical_Beach_3k.hdr");
	SkyLight* skylight = world.m_by_id<SkyLight>(skybox);

	extract_lighting_from_cubemap(renderer->lighting_system, *skylight);

	make_lighting_system(renderer->lighting_system, *skylight);


	{
		for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			renderer->scene_pass_ubo[i] = alloc_ubo_buffer(sizeof(PassUBO), UBO_PERMANENT_MAP);
			renderer->simulation_ubo[i] = alloc_ubo_buffer(sizeof(SimulationUBO), UBO_PERMANENT_MAP);

			DescriptorDesc descriptor_desc = {};
			add_ubo(descriptor_desc, VERTEX_STAGE, renderer->scene_pass_ubo[i], 0);
			add_ubo(descriptor_desc, VERTEX_STAGE | FRAGMENT_STAGE, renderer->simulation_ubo[i], 1);
			update_descriptor_set(renderer->scene_pass_descriptor[i], descriptor_desc);
		}

		renderer->z_prepass_pipeline_layout = query_Layout(renderer->scene_pass_descriptor[0]);

		array<2, descriptor_set_handle> descriptors = { renderer->scene_pass_descriptor[0], renderer->lighting_system.pbr_descriptor };
		renderer->color_pipeline_layout = query_Layout(descriptors);
	}

	for (uint i = 0; i < 4; i++) {
		FramebufferDesc desc{ settings.shadow_resolution , settings.shadow_resolution };
		add_depth_attachment(desc, &renderer->shadow_mask_map[i]);
			
		make_Framebuffer((RenderPass::ID)(RenderPass::Shadow0 + i), desc);
	}

	{
		FramebufferDesc desc{ settings.display_resolution_width, settings.display_resolution_height };
        desc.stencil_buffer = StencilBufferFormat::P8;
        
        uint msaa = 4;
        
		AttachmentDesc& attachment = add_color_attachment(desc, &renderer->scene_map);
        attachment.num_samples = msaa;
        
		add_dependency(desc, VERTEX_STAGE, RenderPass::TerrainHeightGeneration);
		add_dependency(desc, FRAGMENT_STAGE, RenderPass::TerrainTextureGeneration);

		for (uint i = 0; i < 4; i++) {
			add_dependency(desc, FRAGMENT_STAGE, (RenderPass::ID)(RenderPass::Shadow0 + i));
		}

		SubpassDesc subpasses[2] = {};
		SubpassDesc& z_prepass = subpasses[0];
		z_prepass.depth_attachment = true;

		SubpassDesc& color_pass = subpasses[1];
		color_pass.color_attachments.append(0);
		color_pass.depth_attachment = true;

		make_Framebuffer(RenderPass::Scene, desc, { subpasses, 2 });
	}

	init_terrain_render_resources(renderer->terrain_render_resources);

	return renderer;
}

void build_framegraph(Renderer& renderer, slice<Dependency> dependencies) {
	make_wsi_pass(dependencies);
	build_framegraph();
}

void destroy_Renderer(Renderer* renderer) {
	
}

struct GlobalUBO {
	glm::mat4 projection;
	glm::mat4 view;
	float window_width;
	float window_height;
};

struct GlobalSceneData {

};

void render_overlay(Renderer& renderer, RenderPass& render_pass) {
	render_debug_bvh(renderer.scene_partition, render_pass);
}

void extract_render_data(Renderer& renderer, Viewport& viewport, FrameData& frame,  World& world, EntityQuery layermask) {
	static bool updated_acceleration = false;
	if (!updated_acceleration) {
		build_acceleration_structure(renderer.scene_partition, renderer.mesh_buckets, world);
		updated_acceleration = true;
	}
	
	for (uint i = 0; i < RenderPass::ScenePassCount; i++) {
		frame.culled_mesh_bucket[i] = TEMPORARY_ARRAY(CulledMeshBucket, MAX_MESH_BUCKETS);
	}

	//update_camera_matrices(world, layermask, viewport);

	fill_pass_ubo(frame.pass_ubo, viewport);
	fill_light_ubo(frame.light_ubo, world, viewport, layermask);
	cull_meshes(renderer.scene_partition, world, renderer.mesh_buckets, frame.culled_mesh_bucket[RenderPass::Scene], viewport);
	extract_grass_render_data(frame.grass_data, world, &viewport);
	extract_render_data_terrain(frame.terrain_data, world, &viewport, layermask);
	extract_skybox(frame.skybox_data, world, layermask);

}

GPUSubmission build_command_buffers(Renderer& renderer, const FrameData& frame) {
	Profile profile("Begin Render Frame");
	RenderPass screen = begin_render_frame();
	profile.end();

	load_assets_in_queue();
	
	for (UpdateMaterial& material : renderer.update_materials) {
		update_Material(material.handle, material.from, material.to);
	}

	renderer.update_materials.clear();

	//reload_modified_shaders();

	GPUSubmission submission = {
		begin_render_pass(RenderPass::Scene),
		begin_render_pass(RenderPass::Shadow0),
		begin_render_pass(RenderPass::Shadow1),
		begin_render_pass(RenderPass::Shadow2),
		begin_render_pass(RenderPass::Shadow3),
		screen,
	};

	RenderPass& main_pass = submission.render_passes[RenderPass::Scene];
	CommandBuffer& cmd_buffer = main_pass.cmd_buffer;

	uint frame_index = get_frame_index();

	SimulationUBO simulation_ubo = {};
	simulation_ubo.time = Time::now();

	//todo would be more efficient to build structs in place, instead of copying
	memcpy_ubo_buffer(renderer.scene_pass_ubo[frame_index], &frame.pass_ubo);
	memcpy_ubo_buffer(renderer.simulation_ubo[frame_index], &simulation_ubo);
	memcpy_ubo_buffer(renderer.lighting_system.light_ubo, &frame.light_ubo);

	descriptor_set_handle scene_pass_descriptor = renderer.scene_pass_descriptor[frame_index];

	//Z-PREPASS
	
	//Sychronization artifacts in motion
	
	bind_pipeline_layout(main_pass.cmd_buffer, renderer.z_prepass_pipeline_layout);
	bind_descriptor(cmd_buffer, 0, scene_pass_descriptor);

	render_meshes(renderer.mesh_buckets, frame.culled_mesh_bucket[RenderPass::Scene], main_pass);
	render_grass(frame.grass_data, main_pass);
	
	next_subpass(main_pass); 

	array<2, descriptor_set_handle> descriptors = {scene_pass_descriptor, renderer.lighting_system.pbr_descriptor};

	bind_pipeline_layout(cmd_buffer, renderer.color_pipeline_layout);
	bind_descriptor(cmd_buffer, 0, scene_pass_descriptor);
	bind_descriptor(cmd_buffer, 1, renderer.lighting_system.pbr_descriptor);

	render_terrain(renderer.terrain_render_resources, frame.terrain_data, submission.render_passes);

	//todo paritition into lit, unlit, transparent passes
	
	render_meshes(renderer.mesh_buckets, frame.culled_mesh_bucket[RenderPass::Scene], main_pass);
	render_grass(frame.grass_data, main_pass);
	//render_skybox(frame.skybox_data, main_pass);

	return submission;
}



void submit_frame(Renderer& renderer, GPUSubmission& submission) {
	for (uint i = 0; i < RenderPass::ScenePassCount; i++) {
		end_render_pass(submission.render_passes[i]);
	}

	end_render_frame(submission.render_passes[RenderPass::Screen]);
}

/*
RenderPass Renderer::render(World& world, Layermask layermask, uint width, uint height, RenderExtension* ext) {	
	model_m = TEMPORARY_ARRAY(glm::mat4, 1000); //todo find real number

	PreRenderParams pre_render(layermask);

	CommandBuffer cmd_buffer;
	
	RenderPass ctx(cmd_buffer, main_pass);
	ctx.layermask = layermask;
	ctx.pass = main_pass;
	ctx.skybox = world.filter<Skybox>(ctx.layermask)[0];
	ctx.dir_light = world.filter<DirLight>(ctx.layermask)[0];
	ctx.extension = ext;
	ctx.width = width;
	ctx.height = height;



	ID camera_id = get_camera(world, ctx.layermask & EDITOR_LAYER ? EDITOR_LAYER : GAME_LAYER);
	update_camera_matrices(world, camera_id, ctx);	
	
	FrameData data;
	data.model_matrix = glm::mat4(1.0);
	data.proj_matrix = ctx.projection;
	data.view_matrix = ctx.view;
	
	vk_draw_frame(data);
	return ctx;

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
*/
