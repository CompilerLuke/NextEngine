#include "graphics/renderer/terrain.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "core/profiler.h"
#include "components/transform.h"
#include "components/terrain.h"
#include "components/camera.h"
#include "graphics/assets/material.h"
#include "graphics/renderer/renderer.h"
#include "graphics/culling/culling.h"
#include "graphics/rhi/rhi.h"
#include <glm/gtc/matrix_transform.hpp>

//ASYNC COPY

void make_async_copy_resources(AsyncCopyResources& resources, uint size) {
	VkDevice device = rhi.device;
	VkPhysicalDevice physical_device = rhi.device;

	resources.host_visible = make_HostVisibleBuffer(device, physical_device, VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);
	map_buffer_memory(device, resources.host_visible);
}

void destroy_async_copy_resources(AsyncCopyResources& resources) {
	VkDevice device = rhi.device;
	unmap_buffer_memory(device, resources.host_visible);
	vkDestroyFence(device, resources.fence, nullptr);
}

//this is somewhat hacky but it will only send a transfer once the last one has been received
void async_copy_image(VkCommandBuffer cmd_buffer, texture_handle image_handle, AsyncCopyResources& copy) {
	if (copy.transfer_frame != -1) return;
	
	Texture& texture = *get_Texture(image_handle);
	VkImage image = texture.image;

	VkBufferImageCopy region = {};
	region.imageExtent = { (uint)texture.desc.width, (uint)texture.desc.height, 1 };
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.mipLevel = 0;

	vkCmdCopyImageToBuffer(cmd_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, copy.host_visible.buffer, 1, &region);

	copy.transfer_frame = rhi.frame_index;
}

void receive_transfer(AsyncCopyResources& resources, uint size, void* dst) {
	if (resources.transfer_frame == -1) return;
	memcpy(dst, resources.host_visible.mapped, size);
	resources.transfer_frame = -1;
}

model_handle load_subdivided(uint num) {
	return load_Model(tformat("subdivided_plane", num, ".fbx"));
}

//todo instanced rendering is always utilised even when a simple push constant or even uniform buffer
//would be more efficient!
void init_terrain_render_resources(TerrainRenderResources& resources) {
	resources.flat_shader = load_Shader("shaders/pbr.vert", "shaders/gizmo.frag");
	resources.terrain_shader = load_Shader("shaders/terrain.vert", "shaders/terrain.frag");

	resources.subdivided_plane[0] = load_subdivided(32);
	resources.subdivided_plane[1] = load_subdivided(16);
	resources.subdivided_plane[2] = load_subdivided(8);

	{
		MaterialDesc mat_desc{ resources.flat_shader };
		mat_vec3(mat_desc, "color", glm::vec3(1, 1, 0));

		resources.control_point_material = make_Material(mat_desc);
	}

	{
		MaterialDesc mat_desc{ resources.flat_shader };
		//mat_desc.draw_state = Cull_None | PolyMode_Wireframe;

		mat_vec3(mat_desc, "color", glm::vec3(0, 1, 1));

		resources.splat_material = make_Material(mat_desc);
	}

	{
		//todo stencil trick!
		MaterialDesc mat_desc{ resources.flat_shader };
		mat_vec3(mat_desc, "color", glm::vec3(0.7, 0.7, 0));
		mat_desc.draw_state = DepthFunc_Always;
	}

	resources.terrain_ubo = alloc_ubo_buffer(sizeof(TerrainUBO), UBO_MAP_UNMAP);

	resources.displacement_sampler = query_Sampler({Filter::Nearest, Filter::Nearest, Filter::Nearest});
	resources.blend_values_sampler = query_Sampler({Filter::Linear, Filter::Linear, Filter::Linear});
	resources.blend_idx_sampler = query_Sampler({});

	make_async_copy_resources(resources.async_copy, 12*12 * 32*32 * sizeof(float)); //allow resizing of this buffer
}

const uint MAX_TERRAIN_TEXTURES = 10;

void gen_terrain(Terrain& terrain) {
	terrain.displacement_map[0].clear();
	terrain.displacement_map[1].clear();
	terrain.displacement_map[2].clear();
	terrain.blend_idx_map.clear();
	terrain.blend_values_map.clear();

	uint width = terrain.width * 32;
	uint height = terrain.height * 32;

	terrain.displacement_map[0].resize(width * height);
	terrain.displacement_map[1].resize(width * height / 4);
	terrain.displacement_map[2].resize(width * height / 8);
	terrain.blend_idx_map.resize(width * height);
	terrain.blend_values_map.resize(width * height);

	for (uint y = 0; y < height; y++) {
		for (uint x = 0; x < width; x++) {
			float displacement = sin(10.0 * (float)x / width) * sin(10.0 * (float)y / height);
			displacement = displacement * 0.5 + 0.5f;
			
			uint index = y * width + x;

			terrain.displacement_map[0][index] = displacement;
			terrain.blend_idx_map[index] = pack_char_rgb(0, 1, 2, 3);
			terrain.blend_values_map[index] = pack_float_rgb(1.0f - displacement, displacement, 0, 0);
		}
	}

	{
		TerrainMaterial terrain_material;
		terrain_material.diffuse = load_Texture("smkmagnb_4K_Albedo.jpg");
		terrain_material.metallic = load_Texture("black.png");
		terrain_material.roughness = load_Texture("smkmagnb_4K_Roughness.jpg");
		terrain_material.normal = load_Texture("smkmagnb_4K_Normal.jpg");
		terrain_material.height = load_Texture("smkmagnb_4K_Displacement.jpg");
		terrain_material.ao = load_Texture("smkmagnb_4K_AO.jpg");
		terrain.materials.append(terrain_material);
	}

	{
		TerrainMaterial terrain_material;
		terrain_material.diffuse = load_Texture("ground.jpg");
		terrain_material.metallic = load_Texture("wet_street/Pebble_Wet_street_metallic.jpg");
		terrain_material.roughness = load_Texture("wet_street/Pebble_Wet_street_roughness.jpg");
		terrain_material.normal = default_textures.normal;
		terrain_material.height = default_textures.black;
		terrain_material.ao = default_textures.white;
		terrain.materials.append(terrain_material);
	}
}

void update_terrain_material(TerrainRenderResources& resources, Terrain& terrain) {
	uint width = terrain.width * 32;
	uint height = terrain.height * 32;

	//KRIGING GPU GENERATION

	printf("Size of Kriging UBO %i\n", sizeof(KrigingUBO));

	{
		resources.kriging_ubo = alloc_ubo_buffer(sizeof(KrigingUBO), UBO_PERMANENT_MAP);

		DescriptorDesc kriging_desc = {};
		add_ubo(kriging_desc, FRAGMENT_STAGE, resources.kriging_ubo, 0);

		update_descriptor_set(resources.kriging_descriptor, kriging_desc);

		FramebufferDesc framebuffer_desc = { width, height };
		framebuffer_desc.depth_buffer = Disable_Depth_Buffer;

		AttachmentDesc& displacement_attachment = add_color_attachment(framebuffer_desc, &resources.displacement_map);
		displacement_attachment.format = TextureFormat::HDR;
		displacement_attachment.num_channels = 1;
		displacement_attachment.num_mips = 3;
		displacement_attachment.final_layout = TextureLayout::TransferSrcOptimal;

		make_Framebuffer(RenderPass::TerrainHeightGeneration, framebuffer_desc);

		PipelineDesc pipeline_desc = {};
		pipeline_desc.render_pass = render_pass_by_id(RenderPass::TerrainHeightGeneration);
		pipeline_desc.shader = load_Shader("shaders/kriging.vert", "shaders/kriging.frag");

		resources.kriging_pipeline = query_Pipeline(pipeline_desc);
	}
	
	{
		glm::mat4 proj = glm::ortho(0.0f, (float)(width * 2), 0.0f, (float)(height * 2), 0.0f, 1.0f);
		
		resources.splat_ubo = alloc_ubo_buffer(sizeof(glm::mat4), UBO_PERMANENT_MAP);
		memcpy_ubo_buffer(resources.splat_ubo, &proj);

		texture_handle brush = load_Texture("brush_low_res.png");

		DescriptorDesc splat_desc = {};
		add_ubo(splat_desc, VERTEX_STAGE, resources.splat_ubo, 0);
		add_combined_sampler(splat_desc, FRAGMENT_STAGE, resources.displacement_sampler, resources.displacement_map, 1);
		add_combined_sampler(splat_desc, FRAGMENT_STAGE, resources.blend_values_sampler, brush, 2);

		update_descriptor_set(resources.splat_descriptor, splat_desc);

		FramebufferDesc framebuffer_desc = { width, height };
		AttachmentDesc& blend_attachment = add_color_attachment(framebuffer_desc, &resources.blend_values_map);
		blend_attachment.format = TextureFormat::UNORM;
		blend_attachment.num_channels = 4;
		
		add_dependency(framebuffer_desc, FRAGMENT_STAGE, RenderPass::TerrainHeightGeneration);

		make_Framebuffer(RenderPass::TerrainTextureGeneration, framebuffer_desc);

		PipelineDesc pipeline_desc = {};
		pipeline_desc.state = BlendMode_One_Minus_Src_Alpha;
		pipeline_desc.range[1].size = sizeof(SplatPushConstant);
		pipeline_desc.shader = load_Shader("shaders/splat.vert", "shaders/splat.frag");
		pipeline_desc.render_pass = render_pass_by_id(RenderPass::TerrainTextureGeneration);

		resources.splat_pipeline = query_Pipeline(pipeline_desc);
	}

	

	//TERRAIN RESOURCES
	Image displacement_desc{ TextureFormat::HDR, width, height, 1 };
	Image blend_idx_desc{ TextureFormat::U8, width, height, 4 };
	Image blend_map_desc{ TextureFormat::UNORM, width, height, 4 };

	displacement_desc.data = terrain.displacement_map[0].data;
	blend_idx_desc.data = terrain.blend_idx_map.data;
	blend_map_desc.data = terrain.blend_values_map.data;

	//resources.displacement_map = upload_Texture(displacement_desc);
	resources.blend_idx_map = upload_Texture(blend_idx_desc);
	//resources.blend_values_map = upload_Texture(blend_map_desc);
	
	
	CombinedSampler diffuse_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler metallic_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler roughness_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler normal_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler height_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler ao_textures[MAX_TERRAIN_TEXTURES] = {};

	static sampler_handle default_sampler = query_Sampler({});

	for (uint i = 0; i < MAX_TERRAIN_TEXTURES; i++) {
		diffuse_textures[i].sampler = default_sampler;
		diffuse_textures[i].texture = default_textures.white;

		metallic_textures[i].sampler = default_sampler;
		metallic_textures[i].texture = default_textures.white;

		roughness_textures[i].sampler = default_sampler;
		roughness_textures[i].texture = default_textures.white;

		normal_textures[i].sampler = default_sampler;
		normal_textures[i].texture = default_textures.normal;

		height_textures[i].sampler = default_sampler;
		height_textures[i].texture = default_textures.black;

		ao_textures[i].sampler = default_sampler;
		ao_textures[i].texture = default_textures.white;
	}

	for (uint i = 0; i < terrain.materials.length; i++) {
		diffuse_textures[i].texture = terrain.materials[i].diffuse;
		metallic_textures[i].texture = terrain.materials[i].metallic;
		roughness_textures[i].texture = terrain.materials[i].roughness;
		normal_textures[i].texture = terrain.materials[i].normal;
		height_textures[i].texture = terrain.materials[i].height;
		ao_textures[i].texture = terrain.materials[i].ao;
	}

	DescriptorDesc terrain_descriptor = {};
	add_ubo(terrain_descriptor, VERTEX_STAGE, resources.terrain_ubo, 0);
	add_combined_sampler(terrain_descriptor, VERTEX_STAGE, resources.displacement_sampler, resources.displacement_map, 1);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, resources.blend_idx_sampler, resources.blend_idx_map, 2);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, resources.blend_values_sampler, resources.blend_values_map, 3);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { diffuse_textures, MAX_TERRAIN_TEXTURES}, 4);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { metallic_textures, MAX_TERRAIN_TEXTURES}, 5);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { roughness_textures, MAX_TERRAIN_TEXTURES}, 6);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { normal_textures, MAX_TERRAIN_TEXTURES}, 7);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { height_textures, MAX_TERRAIN_TEXTURES}, 8);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { ao_textures, MAX_TERRAIN_TEXTURES}, 9);


	TerrainUBO terrain_ubo; 
	terrain_ubo.max_height = terrain.max_height;
	terrain_ubo.displacement_scale = glm::vec2(1.0 / terrain.width, 1.0 / terrain.height);
	terrain_ubo.transformUVs = glm::vec2(1, 1);
	terrain_ubo.grid_size = terrain.size_of_block / 32.0;

	memcpy_ubo_buffer(resources.terrain_ubo, sizeof(TerrainUBO), &terrain_ubo);

	update_descriptor_set(resources.terrain_descriptor, terrain_descriptor);

	//todo add shader variants for shadow mapping!
	for (uint i = 0; i < 1; i++) {
		PipelineDesc pipeline_desc = {};
		pipeline_desc.shader = resources.terrain_shader;
		pipeline_desc.render_pass = render_pass_by_id((RenderPass::ID)i); 
		pipeline_desc.instance_layout = INSTANCE_LAYOUT_TERRAIN_CHUNK;

		resources.terrain_pipeline[i] = query_Pipeline(pipeline_desc);
	}
}

uint lod_from_dist(float dist) {
	if (dist < 50) return 0;
	else if (dist < 100) return 1;
	else return 2;
}

glm::vec3 position_of_chunk(glm::vec3 position, float size_of_block, uint w, uint h) {
	return position + glm::vec3(w * size_of_block, 0, (h + 1) * size_of_block);
}


void extract_render_data_terrain(TerrainRenderData& render_data, World& world, const Viewport viewports[RenderPass::ScenePassCount], Layermask layermask) {
	glm::vec4 planes[RenderPass::ScenePassCount][6];
	glm::vec3 cam_pos_per_pass[RenderPass::ScenePassCount];

	uint render_pass_count = 1;
	
	for (int i = 0; i < render_pass_count; i++) {
		extract_planes(viewports[i], planes[i]);
		cam_pos_per_pass[i] = viewports[i].cam_pos; //-glm::vec3(viewports[i].view[3]);
	}
	
	//TODO THIS ASSUMES EITHER 0 or 1 TERRAINS
	for (auto [e,self,self_trans] : world.filter<Terrain, Transform>(layermask)) { //todo heavily optimize terrain
		for (uint w = 0; w < self.width; w++) {
			for (uint h = 0; h < self.height; h++) {
				//Calculate position of chunk
				Transform t;
				t.position = position_of_chunk(self_trans.position, self.size_of_block, w, h);
				t.scale = glm::vec3((float)self.size_of_block);
				t.scale.y = 1.0f;

				glm::mat4 model_m = compute_model_matrix(t);
				glm::vec2 displacement_offset = glm::vec2(1.0 / self.width * w, 1.0 / self.height * h);

				//Cull and compute lod of chunk
				AABB aabb;
				aabb.min = glm::vec3(0, 0, -(int)self.size_of_block) + t.position;
				aabb.max = glm::vec3(self.size_of_block, self.max_height, 0) + t.position;

				for (uint pass = 0; pass < render_pass_count; pass++) {
					//if (frustum_test(planes[pass], aabb) == OUTSIDE) continue;

					glm::vec3 cam_pos = cam_pos_per_pass[pass];

					float dist = glm::length(t.position - cam_pos);
					uint lod = lod_from_dist(dist);
					uint edge_lod = 0;

					edge_lod = min(edge_lod, lod_from_dist(glm::length(cam_pos - position_of_chunk(self_trans.position, self.size_of_block, w - 1, h))));
					edge_lod = min(edge_lod, lod_from_dist(glm::length(cam_pos - position_of_chunk(self_trans.position, self.size_of_block, w + 1, h))));
					edge_lod = min(edge_lod, lod_from_dist(glm::length(cam_pos - position_of_chunk(self_trans.position, self.size_of_block, w, h - 1))));
					edge_lod = min(edge_lod, lod_from_dist(glm::length(cam_pos - position_of_chunk(self_trans.position, self.size_of_block, w, h + 1))));

					ChunkInfo chunk_info = {};
					chunk_info.model_m = model_m;
					chunk_info.displacement_offset = displacement_offset;
					chunk_info.lod = lod;
					chunk_info.edge_lod = edge_lod;

					render_data.lod_chunks[pass][lod].append(chunk_info);
				}
			}
		}

		if (self.show_control_points && layermask & EDITOR_LAYER) {
			for (auto [e, control, trans] : world.filter<TerrainControlPoint, Transform>(layermask)) {
				glm::mat4 model_m = compute_model_matrix(trans);
				
				render_data.control_points.append(model_m);
			}

			for (auto[e, control, trans] : world.filter<TerrainSplat, Transform>(layermask)) {
				glm::mat4 model_m = compute_model_matrix(trans);
				model_m = glm::scale(model_m, glm::vec3(0.1, 0.1, 0.1));

				render_data.splat_points.append(model_m);
			}
		}

		break;
	}

	
	
	
}

void clear_image(CommandBuffer& cmd_buffer, texture_handle handle, glm::vec4 color) {
	Texture& texture = *get_Texture(handle);
	
	VkClearColorValue vk_clear_color = {};
	vk_clear_color = { color.x, color.y, color.z, color.a };

	VkImageSubresourceRange range = {};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseArrayLayer = 0;
	range.layerCount = 1;
	range.baseMipLevel = 0;
	range.levelCount = texture.desc.num_mips;

	vkCmdClearColorImage(cmd_buffer, get_Texture(handle)->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vk_clear_color, 1, &range);
}

void clear_undefined_image(CommandBuffer& cmd_buffer, texture_handle handle, glm::vec4 color, TextureLayout final_layout = TextureLayout::ShaderReadOptimal) {
	transition_layout(cmd_buffer, handle, TextureLayout::Undefined, TextureLayout::TransferDstOptimal);
	clear_image(cmd_buffer, handle, color);
	transition_layout(cmd_buffer, handle, TextureLayout::TransferDstOptimal, final_layout);
}

void render_terrain(TerrainRenderResources& resources, const TerrainRenderData& data, RenderPass render_passes[RenderPass::ScenePassCount]) {
	recycle_descriptor_set(resources.terrain_descriptor);
	
	static bool init_terrain = true;

	if (init_terrain) {
		RenderPass clear_height = begin_render_pass(RenderPass::TerrainHeightGeneration, glm::vec4(1.0));
		end_render_pass(clear_height);

		transition_layout(clear_height.cmd_buffer, resources.displacement_map, TextureLayout::TransferSrcOptimal, TextureLayout::ShaderReadOptimal);

		RenderPass clear_blend = begin_render_pass(RenderPass::TerrainTextureGeneration, glm::vec4(1.0, 0.0, 0.0, 0.0));
		end_render_pass(clear_blend);
		
		init_terrain = false;
	}

	for (uint pass = 0; pass < 1; pass++) {
		CommandBuffer& cmd_buffer = render_passes[pass].cmd_buffer;

		bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_TERRAIN_CHUNK);
		bind_pipeline(cmd_buffer, resources.terrain_pipeline[pass]);
		bind_descriptor(cmd_buffer, 2, resources.terrain_descriptor.current);

		for (uint i = 0; i < MAX_TERRAIN_CHUNK_LOD; i++) { //todo heavily optimize terrain
			const tvector<ChunkInfo>& chunk_info = data.lod_chunks[pass][i];
			
			VertexBuffer vertex_buffer = get_VertexBuffer(resources.subdivided_plane[i], 0);
			InstanceBuffer instance_buffer = frame_alloc_instance_buffer<ChunkInfo>(INSTANCE_LAYOUT_TERRAIN_CHUNK, chunk_info);

			draw_mesh(cmd_buffer, vertex_buffer, instance_buffer);
		}

		{
			draw_mesh(cmd_buffer, primitives.sphere, resources.control_point_material, data.control_points);
			draw_mesh(cmd_buffer, primitives.sphere, resources.splat_material, data.splat_points);
		}
	}
}

void receive_generated_heightmap(TerrainRenderResources& resources, Terrain& terrain) {
	uint size = terrain.width * 32 * terrain.height * 32;

	terrain.displacement_map[0].reserve(size);
	receive_transfer(resources.async_copy, size * sizeof(float), terrain.displacement_map[0].data);
}

void gpu_estimate_terrain_surface(TerrainRenderResources& resources, KrigingUBO& ubo) {
	memcpy_ubo_buffer(resources.kriging_ubo, &ubo);
	
	RenderPass render_pass = begin_render_pass(RenderPass::TerrainHeightGeneration);
	CommandBuffer& cmd_buffer = render_pass.cmd_buffer;

	bind_vertex_buffer(render_pass.cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
	bind_pipeline(cmd_buffer, resources.kriging_pipeline);
	bind_descriptor(cmd_buffer, 0, resources.kriging_descriptor);

	draw_mesh(cmd_buffer, primitives.quad_buffer);

	end_render_pass(render_pass);

	async_copy_image(cmd_buffer, resources.displacement_map, resources.async_copy);

	transition_layout(cmd_buffer, resources.displacement_map, TextureLayout::TransferSrcOptimal, TextureLayout::ShaderReadOptimal);
}

void gpu_splat_terrain(TerrainRenderResources& resources, slice<glm::mat4> models, slice<SplatPushConstant> splats) {
	RenderPass render_pass = begin_render_pass(RenderPass::TerrainTextureGeneration, glm::vec4(1.0,0.0,0.0,1.0));
	CommandBuffer& cmd_buffer = render_pass.cmd_buffer;

	bind_vertex_buffer(render_pass.cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
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

//todo split up into generate heightmap and generating texture blends
void regenerate_terrain(World& world, TerrainRenderResources& resources, Layermask layermask) {

	for (auto[e, trans, terrain] : world.filter<Transform, Terrain>(layermask)) {
		auto width_quads = TERRAIN_RESOLUTION * terrain.width;
		auto height_quads = TERRAIN_RESOLUTION * terrain.height;

		receive_transfer(resources.async_copy, width_quads * height_quads * sizeof(float), terrain.displacement_map[0].data);


		Profile edit_terrain("Edit terrain");

		float max_height = terrain.max_height;
		glm::vec3 terrain_position = trans.position;
		float size_per_quad = terrain.size_of_block / 32.0f;

		KrigingUBO kriging_ubo = {};

		for (auto[e, control, trans] : world.filter<TerrainControlPoint, Transform>(EDITOR_LAYER)) {
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

		memcpy_ubo_buffer(resources.splat_ubo, &proj_view);

		for (auto[e, control, trans] : world.filter<TerrainSplat, Transform>(EDITOR_LAYER)) {
			glm::mat4 model = compute_model_matrix(trans);
			model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));

			SplatPushConstant splat;
			splat.radius = trans.scale.x / half_width_size * 50.0f;
			splat.material = control.material;
			splat.hardness = control.hardness;
			splat.min_height = control.min_height;
			splat.max_height = control.max_height;

			splat_model.append(model);
			splat_weights.append(splat);
		}

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

		gpu_splat_terrain(resources, splat_model, splat_weights);

		if (kriging_ubo.N == 4) continue;

		compute_kriging_matrix(kriging_ubo, width_quads, height_quads);

		edit_terrain.end();

		gpu_estimate_terrain_surface(resources, kriging_ubo);
	}
}