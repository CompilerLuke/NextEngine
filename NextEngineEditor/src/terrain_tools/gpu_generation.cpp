#include "terrain_tools/gpu_generation.h"
#include "terrain_tools/kriging.h"
#include "graphics/rhi/async_cpu_copy.h"
#include "graphics/rhi/frame_buffer.h"
#include "graphics/rhi/pipeline.h"
#include "graphics/assets/assets.h"
#include "components/terrain.h"
#include "components/transform.h"
#include "graphics/renderer/terrain.h"
#include "graphics/rhi/primitives.h"
#include <glm/gtc/matrix_transform.hpp>
#include "core/profiler.h"
#include "ecs/ecs.h"
#include <algorithm>

void init_terrain_editor_render_resources(TerrainEditorResources& generation, TerrainRenderResources& resources, Terrain& terrain) {
	if (generation.async_copy) return; //already initialized
	generation.async_copy = make_async_copy_resources(12 * 12 * 32 * 32 * sizeof(float)); //allow resizing of this buffer
	
	uint width = terrain.width * TERRAIN_RESOLUTION;
	uint height = terrain.height * TERRAIN_RESOLUTION;

	{
		FramebufferDesc framebuffer_desc = { width, height };
		framebuffer_desc.depth_buffer = DepthBufferFormat::None;

		AttachmentDesc& displacement_attachment = add_color_attachment(framebuffer_desc, &resources.displacement_map);
		displacement_attachment.format = TextureFormat::HDR;
		displacement_attachment.num_channels = 1;
		displacement_attachment.num_mips = 3;
		displacement_attachment.final_layout = TextureLayout::ColorAttachmentOptimal;

		make_Framebuffer(RenderPass::TerrainHeightGeneration, framebuffer_desc);
	}

	{
		FramebufferDesc framebuffer_desc = { 2*width, 2*height };
		AttachmentDesc& blend_attachment = add_color_attachment(framebuffer_desc, &resources.blend_values_map);
		blend_attachment.format = TextureFormat::UNORM;
		blend_attachment.num_channels = 4;

		add_dependency(framebuffer_desc, FRAGMENT_STAGE, RenderPass::TerrainHeightGeneration, TextureAspect::Color);

		make_Framebuffer(RenderPass::TerrainTextureGeneration, framebuffer_desc);
	}

	{
		Image blend_idx_desc{ TextureFormat::U8, width, height, 4 };
		blend_idx_desc.data = terrain.blend_idx_map.data;

		resources.blend_idx_map = upload_Texture(blend_idx_desc);
	}

	{
		GraphicsPipelineDesc pipeline_desc = {};
		pipeline_desc.render_pass = RenderPass::TerrainHeightGeneration;
		pipeline_desc.shader = load_Shader("shaders/kriging.vert", "shaders/kriging.frag");

		generation.kriging_pipeline = query_Pipeline(pipeline_desc);
		generation.kriging_ubo = alloc_ubo_buffer(sizeof(KrigingUBO), UBO_PERMANENT_MAP);

		DescriptorDesc kriging_desc = {};
		add_ubo(kriging_desc, FRAGMENT_STAGE, generation.kriging_ubo, 0);
		update_descriptor_set(generation.kriging_descriptor, kriging_desc);
	}

	{
		GraphicsPipelineDesc pipeline_desc = {};
		pipeline_desc.state = BlendMode_One_Minus_Src_Alpha;
		pipeline_desc.range[1].size = sizeof(SplatPushConstant);
		pipeline_desc.shader = load_Shader("shaders/splat.vert", "shaders/splat.frag");
		pipeline_desc.render_pass = RenderPass::TerrainTextureGeneration;

		generation.splat_pipeline = query_Pipeline(pipeline_desc);
		generation.splat_ubo = alloc_ubo_buffer(sizeof(glm::mat4), UBO_PERMANENT_MAP);

		texture_handle brush = load_Texture("editor/terrain/brush_low_res.png");

		DescriptorDesc splat_desc = {};
		add_ubo(splat_desc, VERTEX_STAGE, generation.splat_ubo, 0);
		add_combined_sampler(splat_desc, FRAGMENT_STAGE, resources.displacement_sampler, resources.displacement_map, 1);
		add_combined_sampler(splat_desc, FRAGMENT_STAGE, resources.blend_values_sampler, brush, 2);

		update_descriptor_set(generation.splat_descriptor, splat_desc);
	}
}

void clear_terrain_render_pass(TerrainRenderResources& maps) {
	RenderPass clear_height = begin_render_pass(RenderPass::TerrainHeightGeneration, glm::vec4(1.0));
	end_render_pass(clear_height);

	transition_layout(*clear_height.cmd_buffer, maps.displacement_map, TextureLayout::ColorAttachmentOptimal,
                      TextureLayout::ShaderReadOptimal);

	RenderPass clear_blend = begin_render_pass(RenderPass::TerrainTextureGeneration, glm::vec4(1.0, 0.0, 0.0, 0.0));
	end_render_pass(clear_blend);
}


void receive_generated_heightmap(TerrainEditorResources& resources, Terrain& terrain) {
	uint size = terrain.width * 32 * terrain.height * 32;

	terrain.displacement_map[0].reserve(size);
	receive_transfer(*resources.async_copy, size * sizeof(float), terrain.displacement_map[0].data);
}

void gpu_estimate_terrain_surface(TerrainEditorResources& resources, TerrainRenderResources& maps, KrigingUBO& ubo) {
	memcpy_ubo_buffer(resources.kriging_ubo, &ubo);

	RenderPass render_pass = begin_render_pass(RenderPass::TerrainHeightGeneration);
	CommandBuffer& cmd_buffer = *render_pass.cmd_buffer;

	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
	bind_pipeline(cmd_buffer, resources.kriging_pipeline);
	bind_descriptor(cmd_buffer, 0, resources.kriging_descriptor);

	draw_mesh(cmd_buffer, primitives.quad_buffer);

	end_render_pass(render_pass);

	transition_layout(cmd_buffer, maps.displacement_map, TextureLayout::ColorAttachmentOptimal, TextureLayout::TransferSrcOptimal);
	async_copy_image(cmd_buffer, maps.displacement_map, *resources.async_copy, TextureAspect::Color);
	transition_layout(cmd_buffer, maps.displacement_map, TextureLayout::TransferSrcOptimal, TextureLayout::ShaderReadOptimal);
}

void gpu_splat_terrain(TerrainEditorResources& resources, slice<glm::mat4> models, slice<SplatPushConstant> splats) {
	RenderPass render_pass = begin_render_pass(RenderPass::TerrainTextureGeneration, glm::vec4(1.0, 0.0, 0.0, 1.0));
	CommandBuffer& cmd_buffer = *render_pass.cmd_buffer;

	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
	bind_pipeline(cmd_buffer, resources.splat_pipeline);
	bind_descriptor(cmd_buffer, 0, resources.splat_descriptor);

	for (uint i = 0; i < models.length; i++) {
		glm::mat4 model = models[i];
		SplatPushConstant splat = splats[i];

		InstanceBuffer instance = frame_alloc_instance_buffer(INSTANCE_LAYOUT_MAT4X4, slice(model));

		push_constant(cmd_buffer, FRAGMENT_STAGE, 0, &splat);
		draw_mesh(cmd_buffer, primitives.quad_buffer, instance);
	}

	end_render_pass(render_pass);

}

void clear_terrain_heightmap(TerrainRenderResources& resources) {
	RenderPass clear_height = begin_render_pass(RenderPass::TerrainHeightGeneration, glm::vec4(1.0));
	end_render_pass(clear_height);

	transition_layout(*clear_height.cmd_buffer, resources.displacement_map, TextureLayout::ColorAttachmentOptimal,
                      TextureLayout::ShaderReadOptimal);

}

void clear_terrain(TerrainRenderResources& resources) {
	clear_terrain_heightmap(resources);

	RenderPass clear_blend = begin_render_pass(RenderPass::TerrainTextureGeneration, glm::vec4(1.0, 0.0, 0.0, 0.0));
	end_render_pass(clear_blend);
}

//todo split up into generate heightmap and generating texture blends
//is EntityQuery even important here
void regenerate_terrain(World& world, TerrainEditorResources& generation_resources, TerrainRenderResources& terrain_resources, EntityQuery layermask) {
	for (auto [e, trans, terrain] : world.filter<Transform, Terrain>()) {
		auto width_quads = TERRAIN_RESOLUTION * terrain.width;
		auto height_quads = TERRAIN_RESOLUTION * terrain.height;

		receive_generated_heightmap(generation_resources, terrain);

		//receive_transfer(resources.async_copy, width_quads * height_quads * sizeof(float), terrain.displacement_map[0].data);


		Profile edit_terrain("Edit terrain");

		float max_height = terrain.max_height;
		glm::vec3 terrain_position = trans.position;
		float size_per_quad = terrain.size_of_block / 32.0f;

		KrigingUBO kriging_ubo = {};

		for (auto [e, control, trans] : world.filter<TerrainControlPoint, Transform>()) {
			glm::vec2 position = glm::vec2(trans.position.x - terrain_position.x, trans.position.z - terrain_position.z) / size_per_quad;
			float height = trans.position.y - terrain_position.y;

			kriging_ubo.positions[kriging_ubo.N++] = glm::vec4(position, height, 0.0);
		}


		tvector<glm::mat4> splat_model;
		tvector<SplatPushConstant> splat_weights;

		float half_width_size = 0.5f * terrain.width * terrain.size_of_block;
		float half_height_size = 0.5f * terrain.height * terrain.size_of_block;
		glm::vec3 center = terrain_position + glm::vec3(half_width_size, terrain.max_height, half_height_size);

		glm::mat4 proj = glm::ortho(-(float)half_width_size, (float)half_width_size, (float)half_height_size, -(float)half_height_size, 0.0f, terrain.max_height);
		glm::mat4 view = glm::lookAt(center, center + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1.0f));
		glm::mat4 proj_view = proj * view;

		memcpy_ubo_buffer(generation_resources.splat_ubo, &proj_view);

		for (auto [e, control, trans] : world.filter<TerrainSplat, Transform>()) {
			glm::mat4 model = compute_model_matrix(trans);
			model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));

			SplatPushConstant splat;
			splat.radius = trans.scale.x / half_width_size * 50.0f;
			splat.material = control.material;
			splat.hardness = control.hardness;
			splat.min_height = control.min_height;
			splat.max_height = control.max_height;

			glm::vec4 pos = proj_view * model * glm::vec4(glm::vec3(0),1);
			pos /= pos.w;

			splat_model.append(model);
			splat_weights.append(splat);
		}

		tvector<int> ids;
		tvector<glm::mat4> sorted_model;
		tvector<SplatPushConstant> sorted_weights;

		ids.reserve(splat_model.length);
		sorted_model.reserve(splat_model.length);
		sorted_weights.reserve(splat_model.length);

		for (uint i = 0; i < splat_model.length; i++) ids.append(i);

		std::sort(ids.begin(), ids.end(), [&splat_model](int a, int b) { return splat_model[a][3][1] < splat_model[b][3][1]; });

		for (uint i : ids) sorted_model.append(splat_model[i]);
		for (uint i : ids) sorted_weights.append(splat_weights[i]);

		uint index = kriging_ubo.N;
		kriging_ubo.positions[kriging_ubo.N++] = glm::vec4(0, 0, 0, 0);
		kriging_ubo.positions[kriging_ubo.N++] = glm::vec4(width_quads, 0, 0, 0);
		kriging_ubo.positions[kriging_ubo.N++] = glm::vec4(0, height_quads, 0, 0);
		kriging_ubo.positions[kriging_ubo.N++] = glm::vec4(width_quads, height_quads, 0, 0);

		auto& heightmap = terrain.displacement_map;
		auto& blend_idx = terrain.blend_idx_map;
		auto& blend_values = terrain.blend_values_map;

		uint size = width_quads * height_quads;

		//heightmap.resize(size);
		//blend_idx.resize(size);
		//blend_values.resize(size);		

		//heightmap.clear();
		//blend_idx.clear();
		//blend_values.clear();

		edit_terrain.end();

		if (kriging_ubo.N > 4) {
			compute_kriging_matrix(kriging_ubo, width_quads, height_quads);
			gpu_estimate_terrain_surface(generation_resources, terrain_resources, kriging_ubo);
		}
		else {
			clear_terrain_heightmap(terrain_resources);
		}

		gpu_splat_terrain(generation_resources, sorted_model, sorted_weights);
	}
}
