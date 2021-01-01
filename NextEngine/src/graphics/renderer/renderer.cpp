#pragma once

#include "engine/engine.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/assets.h"
#include "components/camera.h"
#include "graphics/rhi/draw.h"
#include "core/memory/linear_allocator.h"
#include "graphics/pass/pass.h"
#include "graphics/rhi/window.h"
#include "graphics/rhi/frame_buffer.h"
#include "graphics/culling/culling.h"
#include "core/profiler.h"
#include "core/time.h"

//HACK ACKWARD INITIALIZATION
#include "ecs/ecs.h"

//todo move into rhi.h
#include "graphics/rhi/vulkan/vulkan.h"
uint get_frame_index() {
	return rhi.frame_index;
}

texture_handle get_output_map(Renderer& renderer) {
	//return renderer.scene_map;
	return renderer.composite_resources.composite_map;
}

void make_scene_pass(Renderer& renderer, uint width, uint height, uint msaa) {
	for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		renderer.scene_pass_ubo[i] = alloc_ubo_buffer(sizeof(PassUBO), UBO_PERMANENT_MAP);
		renderer.simulation_ubo[i] = alloc_ubo_buffer(sizeof(SimulationUBO), UBO_PERMANENT_MAP);

		DescriptorDesc descriptor_desc = {};
		add_ubo(descriptor_desc, VERTEX_STAGE, renderer.scene_pass_ubo[i], 0);
		add_ubo(descriptor_desc, VERTEX_STAGE | FRAGMENT_STAGE, renderer.simulation_ubo[i], 1);
		update_descriptor_set(renderer.scene_pass_descriptor[i], descriptor_desc);
	}
	
	FramebufferDesc desc{ width, height };
	desc.stencil_buffer = StencilBufferFormat::P8;

	AttachmentDesc& attachment = add_color_attachment(desc, &renderer.scene_map);
	attachment.num_samples = msaa; //todo implement msaa for depth settings.msaa;

	AttachmentDesc& depth_attachment = add_depth_attachment(desc, &renderer.depth_scene_map);
	depth_attachment.num_samples = msaa;

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

	renderer.z_prepass_pipeline_layout = query_Layout(renderer.scene_pass_descriptor[0]);
}

Renderer* make_Renderer(const RenderSettings& settings, World& world) {
	Renderer* renderer = PERMANENT_ALLOC(Renderer);
	renderer->settings = settings;
    
	uint width = settings.display_resolution_width;
	uint height = settings.display_resolution_height;

	make_scene_pass(*renderer, width, height, settings.msaa);
    
	return renderer;

	ID skybox = make_default_Skybox(world, "engine/Tropical_Beach_3k.hdr");
	SkyLight* skylight = world.m_by_id<SkyLight>(skybox);
    
	extract_lighting_from_cubemap(renderer->lighting_system, *skylight);

	make_shadow_resources(renderer->shadow_resources, renderer->simulation_ubo, settings.shadow);
	make_lighting_system(renderer->lighting_system, renderer->shadow_resources, *skylight);

	array<2, descriptor_set_handle> descriptors = { renderer->scene_pass_descriptor[0], renderer->lighting_system.pbr_descriptor[0] };
	renderer->color_pipeline_layout = query_Layout(descriptors);

	make_volumetric_resources(renderer->volumetric_resources, renderer->depth_scene_map, renderer->shadow_resources, 0.5*width, 0.5*height);
	make_composite_resources(renderer->composite_resources, renderer->depth_scene_map, renderer->scene_map, renderer->volumetric_resources.volumetric_map, renderer->volumetric_resources.cloud_map, width, height);
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

void extract_render_data(Renderer& renderer, Viewport& viewport, FrameData& frame,  World& world, EntityQuery layermask, EntityQuery camera_layermask) {
	static bool updated_acceleration = false;
	if (!updated_acceleration) {
		build_acceleration_structure(renderer.scene_partition, renderer.mesh_buckets, world);
		updated_acceleration = true;
	}
	
	for (uint i = 0; i < RenderPass::ScenePassCount; i++) {
		frame.culled_mesh_bucket[i] = TEMPORARY_ARRAY(CulledMeshBucket, MAX_MESH_BUCKETS);
	}

	update_camera_matrices(world, camera_layermask, viewport);
	extract_planes(viewport);
	
	Viewport viewports[RenderPass::ScenePassCount];
	viewports[0] = viewport;

	fill_pass_ubo(frame.pass_ubo, viewport);
	fill_light_ubo(frame.light_ubo, world, viewport, layermask);
	extract_shadow_cascades(frame.shadow_proj_info, viewports + 1, renderer.settings.shadow, world, viewport, camera_layermask);
	fill_volumetric_ubo(frame.volumetric_ubo, frame.composite_ubo, world, renderer.settings.volumetric, viewport, camera_layermask);
	fill_composite_ubo(frame.composite_ubo, viewport);

	cull_meshes(renderer.scene_partition, world, renderer.mesh_buckets, RenderPass::ScenePassCount, frame.culled_mesh_bucket, viewports, layermask);
		
	extract_grass_render_data(frame.grass_data, world, viewports);
	extract_render_data_terrain(frame.terrain_data, world, &viewport, layermask);
	extract_skybox(frame.skybox_data, world, layermask);

}

void bind_scene_pass_z_prepass(Renderer& renderer, RenderPass& render_pass, const FrameData& frame) {
	uint frame_index = get_frame_index();
	
	SimulationUBO simulation_ubo = {};
	simulation_ubo.time = Time::now();
	memcpy_ubo_buffer(renderer.scene_pass_ubo[frame_index], &frame.pass_ubo);
	memcpy_ubo_buffer(renderer.simulation_ubo[frame_index], &simulation_ubo);

	descriptor_set_handle scene_pass_descriptor = renderer.scene_pass_descriptor[frame_index];

	bind_pipeline_layout(render_pass.cmd_buffer, renderer.z_prepass_pipeline_layout);
	bind_descriptor(render_pass.cmd_buffer, 0, scene_pass_descriptor);
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

	if (renderer.settings.hotreload_shaders) {
		reload_modified_shaders();
	}

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

	//todo move into Frame struct
	ShadowUBO shadow_ubo = {};
	fill_shadow_ubo(shadow_ubo, frame.shadow_proj_info);

	//todo would be more efficient to build structs in place, instead of copying
	memcpy_ubo_buffer(renderer.lighting_system.light_ubo[frame_index], &frame.light_ubo);
	memcpy_ubo_buffer(renderer.shadow_resources.shadow_ubos[frame_index], &shadow_ubo);
	memcpy_ubo_buffer(renderer.composite_resources.ubo[frame_index], &frame.composite_ubo);

	//SHADOW PASS
	bind_cascade_viewports(renderer.shadow_resources, submission.render_passes + RenderPass::Shadow0, renderer.settings.shadow, frame.shadow_proj_info);
	
	for (uint i = 0; i < MAX_SHADOW_CASCADES; i++) {
		RenderPass& render_pass = submission.render_passes[RenderPass::Shadow0 + i];
		render_meshes(renderer.mesh_buckets, frame.culled_mesh_bucket[RenderPass::Shadow0 + i], render_pass);
		render_grass(frame.grass_data, render_pass);
	}

	//Z-PREPASS	
	bind_scene_pass_z_prepass(renderer, main_pass, frame);

	render_meshes(renderer.mesh_buckets, frame.culled_mesh_bucket[RenderPass::Scene], main_pass);
	render_grass(frame.grass_data, main_pass);
	
	next_subpass(main_pass); 

	//MAIN PASS
	bind_pipeline_layout(cmd_buffer, renderer.color_pipeline_layout);
	bind_descriptor(cmd_buffer, 1, renderer.lighting_system.pbr_descriptor[frame_index]);

	render_terrain(renderer.terrain_render_resources, frame.terrain_data, submission.render_passes);

	//todo paritition into lit, unlit, transparent passes
	
	render_meshes(renderer.mesh_buckets, frame.culled_mesh_bucket[RenderPass::Scene], main_pass);
	render_grass(frame.grass_data, main_pass);
	//render_skybox(frame.skybox_data, main_pass);

	//Post-Processing
	render_volumetric_pass(renderer.volumetric_resources, frame.volumetric_ubo);
	render_composite_pass(renderer.composite_resources);

	return submission;
}



void submit_frame(Renderer& renderer, GPUSubmission& submission) {
	for (uint i = 0; i < RenderPass::ScenePassCount; i++) {
		end_render_pass(submission.render_passes[i]);
	}

	end_render_frame(submission.screen_render_pass);
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
