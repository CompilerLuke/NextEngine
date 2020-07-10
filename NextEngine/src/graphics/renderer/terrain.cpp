#include "graphics/renderer/terrain.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "core/io/logger.h"
#include "components/transform.h"
#include "components/terrain.h"
#include "components/camera.h"
#include "graphics/assets/material.h"
#include "graphics/renderer/renderer.h"
#include "graphics/rhi/rhi.h"

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

	MaterialDesc mat_desc{ resources.flat_shader };
	mat_vec3(mat_desc, "color", glm::vec3(1, 1, 0));

	resources.control_point_material = make_Material(mat_desc);

	resources.terrain_ubo = alloc_ubo_buffer(sizeof(TerrainUBO), UBO_MAP_UNMAP);

	SamplerDesc linear_desc{ Filter::Linear, Filter::Linear, Filter::Linear, Wrap::ClampToBorder, Wrap::ClampToBorder };

	resources.displacement_sampler = query_Sampler(linear_desc);
	resources.blend_values_sampler = resources.displacement_sampler;
	resources.blend_idx_sampler = query_Sampler({});
}

const uint MAX_TERRAIN_TEXTURES = 10;

void gen_terrain(Terrain& terrain) {
	terrain.displacement_map.clear();
	terrain.blend_idx_map.clear();
	terrain.blend_values_map.clear();

	uint width = terrain.width * 32;
	uint height = terrain.height * 32;

	terrain.displacement_map.resize(width * height);
	terrain.blend_idx_map.resize(width * height);
	terrain.blend_values_map.resize(width * height);

	for (uint y = 0; y < height; y++) {
		for (uint x = 0; x < width; x++) {
			float displacement = sin(10.0 * (float)x / width) * sin(10.0 * (float)y / height);
			displacement = displacement * 0.5 + 0.5f;
			
			uint index = y * width + x;

			pixel blend = {};
			blend.r = (1.0 - displacement) * 255;
			blend.g = displacement * 255;
	
			pixel blend_idx = {};
			blend_idx.r = 1;
			blend_idx.g = 0;
			blend_idx.b = 0;


			terrain.displacement_map[index] = displacement;
			terrain.blend_idx_map[index] = blend_idx;
			terrain.blend_values_map[index] = blend;
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
		terrain_material.diffuse = load_Texture("wet_street/Pebble_Wet_street_basecolor.jpg");
		terrain_material.metallic = load_Texture("wet_street/Pebble_Wet_street_metallic.jpg");
		terrain_material.roughness = load_Texture("wet_street/Pebble_Wet_street_roughness.jpg");
		terrain_material.normal = load_Texture("wet_street/Pebble_Wet_street_normal.jpg");
		terrain_material.height = load_Texture("solid_white.png");
		terrain_material.ao = load_Texture("solid_white.png");
		terrain.materials.append(terrain_material);
	}


	terrain.max_height = 1;
}

void update_terrain_material(TerrainRenderResources& resources, Terrain& terrain) {
	gen_terrain(terrain);

	uint width = terrain.width * 32;
	uint height = terrain.height * 32;

	Image displacement_desc{ TextureFormat::HDR, width, height, 1 };
	Image blend_idx_desc{ TextureFormat::U8, width, height, 4 };
	Image blend_map_desc{ TextureFormat::UNORM, width, height, 4 };

	displacement_desc.data = terrain.displacement_map.data;
	blend_idx_desc.data = terrain.blend_idx_map.data;
	blend_map_desc.data = terrain.blend_values_map.data;

	resources.displacement_map = upload_Texture(displacement_desc);
	resources.blend_idx_map = upload_Texture(blend_idx_desc);
	resources.blend_values_map = upload_Texture(blend_map_desc);
	
	
	CombinedSampler diffuse_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler metallic_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler roughness_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler normal_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler height_textures[MAX_TERRAIN_TEXTURES] = {};
	CombinedSampler ao_textures[MAX_TERRAIN_TEXTURES] = {};

	static texture_handle white_texture = load_Texture("solid_white.png");
	static texture_handle normal_texture = load_Texture("normal.jpg");
	static sampler_handle default_sampler = query_Sampler({});

	for (uint i = 0; i < MAX_TERRAIN_TEXTURES; i++) {
		diffuse_textures[i].sampler = default_sampler;
		diffuse_textures[i].texture = white_texture;

		metallic_textures[i].sampler = default_sampler;
		metallic_textures[i].texture = white_texture;

		roughness_textures[i].sampler = default_sampler;
		roughness_textures[i].texture = white_texture;

		normal_textures[i].sampler = default_sampler;
		normal_textures[i].texture = normal_texture;

		height_textures[i].sampler = default_sampler;
		height_textures[i].texture = white_texture;

		ao_textures[i].sampler = default_sampler;
		ao_textures[i].texture = white_texture;
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

void extract_render_data_terrain(TerrainRenderData& render_data, World& world, const Viewport viewports[RenderPass::ScenePassCount], Layermask layermask) {
	glm::vec4 planes[RenderPass::ScenePassCount][6];
	glm::vec3 cam_pos[RenderPass::ScenePassCount];

	uint render_pass_count = 1;
	
	for (int i = 0; i < render_pass_count; i++) {
		extract_planes(viewports[i], planes[i]);
		cam_pos[i] = -glm::vec3(viewports[i].view[3]);		
	}
	
	//TODO THIS ASSUMES EITHER 0 or 1 TERRAINS
	for (auto [e,self,self_trans] : world.filter<Terrain, Transform>(layermask)) { //todo heavily optimize terrain
		for (uint w = 0; w < self.width; w++) {
			for (uint h = 0; h < self.height; h++) {
				//Calculate position of chunk
				Transform t;
				t.position = self_trans.position + glm::vec3(w * self.size_of_block, 0, (h + 1) * self.size_of_block);
				t.scale = glm::vec3((float)self.size_of_block);

				ChunkInfo chunk_info;
				chunk_info.model_m = compute_model_matrix(t);
				chunk_info.displacement_offset = glm::vec2(1.0 / self.width * w, 1.0 / self.height * h);

				//Cull and compute lod of chunk
				AABB aabb;
				aabb.min = glm::vec3(0, 0, -(int)self.size_of_block) + t.position;
				aabb.max = glm::vec3(self.size_of_block, self.max_height, 0) + t.position;

				for (uint pass = 0; pass < render_pass_count; pass++) {
					//if (frustum_test(planes[pass], aabb) == OUTSIDE) continue;

					float dist = glm::length(t.position - cam_pos[pass]);
					uint lod = lod_from_dist(dist);

					render_data.lod_chunks[pass][lod].append(chunk_info);
				}
			}
		}

		break;
	}

	/*
	if (self->show_control_points && (render_ctx.layermask & EDITOR_LAYER || render_ctx.layermask && PICKING_LAYER)) {
	for (ID id : world.filter<TerrainControlPoint, Transform>(render_ctx.layermask)) {
	Transform* trans = world.by_id<Transform>(id);
	trans->scale = glm::vec3(0.1);

	world.by_id<Entity>(id)->layermask |= PICKING_LAYER;

	glm::mat4 model_m = trans->compute_model_matrix();
	RHI::model_manager.get(cube_model)->render(id, model_m, control_point_materials, render_ctx);
	}
	}
	*/
}

void render_terrain(TerrainRenderResources& resources, const TerrainRenderData& data, RenderPass render_passes[RenderPass::ScenePassCount]) {
	for (uint pass = 0; pass < 1; pass++) {
		CommandBuffer& cmd_buffer = render_passes[pass].cmd_buffer;

		bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_TERRAIN_CHUNK);
		bind_pipeline(cmd_buffer, resources.terrain_pipeline[pass]);
		bind_descriptor(cmd_buffer, 2, resources.terrain_descriptor);

		for (uint i = 0; i < MAX_TERRAIN_CHUNK_LOD; i++) { //todo heavily optimize terrain
			const tvector<ChunkInfo>& chunk_info = data.lod_chunks[pass][i];
			
			VertexBuffer vertex_buffer = get_VertexBuffer(resources.subdivided_plane[i], 0);
			InstanceBuffer instance_buffer = frame_alloc_instance_buffer<ChunkInfo>(INSTANCE_LAYOUT_TERRAIN_CHUNK, chunk_info);

			draw_mesh(cmd_buffer, vertex_buffer, instance_buffer);
		}
	}
}



