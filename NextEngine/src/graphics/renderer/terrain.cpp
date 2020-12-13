#include "core/profiler.h"
#include "ecs/ecs.h"
#include "graphics/renderer/terrain.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "components/transform.h"
#include "components/terrain.h"
#include "components/camera.h"
#include "graphics/assets/material.h"
#include "graphics/renderer/renderer.h"
#include "graphics/culling/culling.h"
#include "graphics/rhi/rhi.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "graphics/rhi/vulkan/texture.h"
#include "graphics/rhi/vulkan/draw.h"

model_handle load_subdivided(uint num) {
	return load_Model(tformat("engine/subdivided_plane", num, ".fbx"));
}

//todo instanced rendering is always utilised even when a simple push constant or even uniform buffer
//would be more efficient!
void init_terrain_render_resources(TerrainRenderResources& resources) {
	resources.terrain_shader = load_Shader("shaders/terrain.vert", "shaders/terrain.frag");

	resources.subdivided_plane[0] = load_subdivided(32);
	resources.subdivided_plane[1] = load_subdivided(16);
	resources.subdivided_plane[2] = load_subdivided(8);

	resources.displacement_sampler = query_Sampler({Filter::Linear, Filter::Linear, Filter::Linear});
	resources.blend_values_sampler = query_Sampler({Filter::Linear, Filter::Linear, Filter::Linear});
	resources.blend_idx_sampler = query_Sampler({Filter::Nearest, Filter::Nearest, Filter::Nearest});

	//todo add shader variants for shadow mapping!
	GraphicsPipelineDesc pipeline_desc = {};
	pipeline_desc.shader = resources.terrain_shader;
	pipeline_desc.render_pass = RenderPass::Shadow0;
	pipeline_desc.shader_flags = SHADER_INSTANCED | SHADER_DEPTH_ONLY;
	pipeline_desc.instance_layout = INSTANCE_LAYOUT_TERRAIN_CHUNK;

	resources.depth_terrain_pipeline = query_Pipeline(pipeline_desc);
	
	pipeline_desc.render_pass = RenderPass::Scene;
	resources.depth_terrain_prepass_pipeline = query_Pipeline(pipeline_desc);

	pipeline_desc.shader_flags = SHADER_INSTANCED;
	pipeline_desc.subpass = 1;
	resources.color_terrain_pipeline = query_Pipeline(pipeline_desc);

	resources.ubo = alloc_ubo_buffer(sizeof(TerrainUBO), UBO_PERMANENT_MAP);
}

const uint MAX_TERRAIN_TEXTURES = 2;
const uint TERRAIN_SUBDIVISION = 32;

void clear_terrain(Terrain& terrain) {
	terrain.displacement_map[0].clear();
	terrain.displacement_map[1].clear();
	terrain.displacement_map[2].clear();
	terrain.blend_idx_map.clear();
	terrain.blend_values_map.clear();
	//terrain.materials.clear(); todo create new function 

	uint width = terrain.width * 32;
	uint height = terrain.height * 32;

	terrain.displacement_map[0].resize(width * height);
	terrain.displacement_map[1].resize(width * height / 4);
	terrain.displacement_map[2].resize(width * height / 8);
	terrain.blend_idx_map.resize(width * height);
	terrain.blend_values_map.resize(width * height);
}

//todo should this function clear the material
//or only set material if it's empty?
void default_terrain_material(Terrain& terrain) {
	if (terrain.materials.length > 0) return;
    
    TerrainMaterial terrain_material;
    terrain_material.diffuse = default_textures.checker;
    terrain_material.metallic = default_textures.black;
    terrain_material.roughness = default_textures.white;
    terrain_material.normal = default_textures.normal;
    terrain_material.height = default_textures.white;
    terrain_material.ao = default_textures.white;
    terrain.materials.append(terrain_material);
	
    /*
	{

		TerrainMaterial terrain_material;
		terrain_material.diffuse = load_Texture("grassy_ground/GrassyGround_basecolor.jpg");
		terrain_material.metallic = load_Texture("grassy_ground/GrassyGround_metallic.jpg");
		terrain_material.roughness = load_Texture("grassy_ground/GrassyGround_roughness.jpg");
		terrain_material.normal = load_Texture("grassy_ground/GrassyGround_normal.jpg");
		terrain_material.height = load_Texture("grassy_ground/GrassyGround_height.jpg");
		terrain_material.ao = default_textures.white;
		terrain.materials.append(terrain_material);
	}

	{
		TerrainMaterial terrain_material;
		terrain_material.diffuse = load_Texture("rock_ground/RockGround_basecolor.jpg");
		terrain_material.metallic = load_Texture("rock_ground/RockGround_metallic.jpg");
		terrain_material.roughness = load_Texture("rock_ground/RockGround_roughness.jpg");
		terrain_material.normal = load_Texture("rock_ground/RockGround_normal.jpg");
		terrain_material.height = default_textures.white;
		terrain_material.ao = default_textures.white;
		terrain.materials.append(terrain_material);
	}
    */
}

void default_terrain(Terrain& terrain) {
	clear_terrain(terrain);

	uint width = terrain.width * 32;
	uint height = terrain.height * 32;

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

	default_terrain_material(terrain);
}

void update_terrain_material(TerrainRenderResources& resources, Terrain& terrain) {
	uint width = terrain.width * 32;
	uint height = terrain.height * 32;

	Image displacement_desc{ TextureFormat::HDR, width, height, 1 };
	Image blend_idx_desc{ TextureFormat::U8, width, height, 4 };
	Image blend_map_desc{ TextureFormat::UNORM, width, height, 4 };

	displacement_desc.data = terrain.displacement_map[0].data;
	blend_idx_desc.data = terrain.blend_idx_map.data;
	blend_map_desc.data = terrain.blend_values_map.data;

	CombinedSampler diffuse_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler metallic_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler roughness_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler normal_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler height_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler ao_textures[MAX_TERRAIN_TEXTURES] = {};

	static sampler_handle default_sampler = query_Sampler({});

	for (uint i = 0; i < MAX_TERRAIN_TEXTURES; i++) {
		diffuse_textures[i] = { default_sampler, default_textures.white };
		metallic_textures[i] = { default_sampler, default_textures.white };
		roughness_textures[i] = { default_sampler, default_textures.white };
		normal_textures[i] = { default_sampler, default_textures.normal };
		height_textures[i] = { default_sampler, default_textures.black };
		ao_textures[i] = { default_sampler, default_textures.white };
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
	add_ubo(terrain_descriptor, VERTEX_STAGE, resources.ubo, 0);
	add_combined_sampler(terrain_descriptor, VERTEX_STAGE, resources.displacement_sampler, resources.displacement_map, 1);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, resources.blend_idx_sampler, resources.blend_idx_map, 2);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, resources.blend_values_sampler, resources.blend_values_map, 3);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { diffuse_textures, MAX_TERRAIN_TEXTURES }, 4);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { metallic_textures, MAX_TERRAIN_TEXTURES }, 5);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { roughness_textures, MAX_TERRAIN_TEXTURES }, 6);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { normal_textures, MAX_TERRAIN_TEXTURES }, 7);
	add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { height_textures, MAX_TERRAIN_TEXTURES }, 8);
	//add_combined_sampler(terrain_descriptor, FRAGMENT_STAGE, { ao_textures, MAX_TERRAIN_TEXTURES}, 9);


	TerrainUBO terrain_ubo;
	terrain_ubo.max_height = terrain.max_height;
	terrain_ubo.displacement_scale = glm::vec2(1.0 / terrain.width, 1.0 / terrain.height);
	terrain_ubo.transformUVs = glm::vec2(1, 1);
	terrain_ubo.grid_size = terrain.size_of_block * terrain.width;

	memcpy_ubo_buffer(resources.ubo, sizeof(TerrainUBO), &terrain_ubo);
	update_descriptor_set(resources.descriptor, terrain_descriptor); 
}

uint lod_from_dist(float dist) {
	if (dist < 50) return 0;
	else if (dist < 100) return 1;
	else return 2;
}

glm::vec3 position_of_chunk(glm::vec3 position, float size_of_block, uint w, uint h) {
	return position + glm::vec3(w * size_of_block, 0, (h + 1) * size_of_block);
}

void extract_render_data_terrain(TerrainRenderData& render_data, World& world, const Viewport viewports[RenderPass::ScenePassCount], EntityQuery layermask) {
	uint render_pass_count = 1;

	//TODO THIS ASSUMES EITHER 0 or 1 TERRAINS
	for (auto[e, self, self_trans] : world.filter<Terrain, Transform>(layermask)) { //todo heavily optimize terrain
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
					if (frustum_test(viewports[pass].frustum_planes, aabb) == OUTSIDE) continue;

					glm::vec3 cam_pos = viewports[pass].cam_pos;

					float dist = glm::length(t.position - cam_pos);
					uint lod = lod_from_dist(dist);
					uint edge_lod = lod;
					edge_lod = max(edge_lod, lod_from_dist(glm::length(cam_pos - position_of_chunk(self_trans.position, self.size_of_block, w - 1, h))));
					edge_lod = max(edge_lod, lod_from_dist(glm::length(cam_pos - position_of_chunk(self_trans.position, self.size_of_block, w + 1, h))));
					edge_lod = max(edge_lod, lod_from_dist(glm::length(cam_pos - position_of_chunk(self_trans.position, self.size_of_block, w, h - 1))));
					edge_lod = max(edge_lod, lod_from_dist(glm::length(cam_pos - position_of_chunk(self_trans.position, self.size_of_block, w, h + 1))));

					ChunkInfo chunk_info = {};
					chunk_info.model_m = model_m;
					chunk_info.displacement_offset = displacement_offset;
					chunk_info.lod = lod;
					chunk_info.edge_lod = edge_lod;

					render_data.lod_chunks[pass][lod].append(chunk_info);
				}
			}
		}
	}
}

//todo move into RHI
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
	if (!resources.descriptor.current.id) return;
	recycle_descriptor_set(resources.descriptor);
	
	for (uint pass = 0; pass < 1; pass++) {
		CommandBuffer& cmd_buffer = render_passes[pass].cmd_buffer;

		bool is_depth = render_passes[pass].type == RenderPass::Depth; //todo extract info function
		bool is_depth_prepass = is_depth && render_passes[pass].id == RenderPass::Scene;

		bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_TERRAIN_CHUNK);
		bind_pipeline(cmd_buffer, is_depth_prepass ? resources.depth_terrain_prepass_pipeline : is_depth ? resources.depth_terrain_pipeline : resources.color_terrain_pipeline);
		bind_descriptor(cmd_buffer, 2, resources.descriptor.current);

		for (uint i = 0; i < MAX_TERRAIN_CHUNK_LOD; i++) { //todo heavily optimize terrain
			const tvector<ChunkInfo>& chunk_info = data.lod_chunks[pass][i];
			
			VertexBuffer vertex_buffer = get_vertex_buffer(resources.subdivided_plane[i], 0);
			InstanceBuffer instance_buffer = frame_alloc_instance_buffer<ChunkInfo>(INSTANCE_LAYOUT_TERRAIN_CHUNK, chunk_info);

			draw_mesh(cmd_buffer, vertex_buffer, instance_buffer);
		}
	}
}