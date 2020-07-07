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

texture_handle alloc_displacement_texture(uint width, uint height) {
	unsigned int texture_id;

	//todo add float texture format
	TextureDesc tex_desc;
	tex_desc.num_channels = 1;
	tex_desc.format = TextureFormat::UNORM;

	uint width_quads = 32 * width;
	uint height_quads = 32 * height;

	return { INVALID_HANDLE }; //alloc_Texture(width_quads, height_quads, tex_desc);
}


void init_terrain_render_resources(TerrainRenderResources& resources) {
	resources.flat_shader = load_Shader("shaders/pbr.vert", "shaders/gizmo.frag");
	resources.terrain_shader = load_Shader("shaders/terrain.vert", "shaders/terrain.frag");

	resources.subdivided_plane[0] = load_subdivided(32);
	resources.subdivided_plane[1] = load_subdivided(16);
	resources.subdivided_plane[2] = load_subdivided(8);

	MaterialDesc mat_desc{ resources.flat_shader };
	mat_vec3(mat_desc, "color", glm::vec3(1, 1, 0));

	resources.control_point_material = make_Material(mat_desc);

	//todo move out
	resources.displacement = alloc_displacement_texture(10, 10);

	resources.terrain_ubo = alloc_ubo_buffer(sizeof(TerrainUBO), UBO_MAP_UNMAP);

	SamplerDesc sampler_desc;
	sampler_desc.wrap_u = Wrap::ClampToBorder;
	sampler_desc.wrap_v = Wrap::ClampToBorder;
	sampler_desc.min_filter = Filter::Linear;
	sampler_desc.mag_filter = Filter::Linear;
	sampler_desc.mip_mode = Filter::Linear;

	sampler_handle sampler = query_Sampler(sampler_desc);

	//vector<VertexAttrib> chunk_info_layout = INSTANCE_MAT4X4_LAYOUT;
	//chunk_info_layout.append({2, Float, offsetof(ChunkInfo, displacement_offset)});
	//chunk_info_layout.append({1, Int, offsetof(ChunkInfo, lod)});
}

void update_terrain_material(TerrainRenderResources& resources, Terrain& terrain, material_handle mat_handle, RenderPass render_pass[RenderPass::ScenePassCount]) {
	DescriptorDesc terrain_descriptor = {};
	add_ubo(terrain_descriptor, VERTEX_STAGE, resources.terrain_ubo, 1);
	add_combined_sampler(terrain_descriptor, VERTEX_STAGE, resources.displacement_sampler, resources.displacement, 2);

	TerrainUBO terrain_ubo; 
	terrain_ubo.max_height = terrain.max_height;
	terrain_ubo.displacement_scale = glm::vec2(1.0 / terrain.width, 1.0 / terrain.height);

	memcpy_ubo_buffer(resources.terrain_ubo, sizeof(TerrainUBO), &terrain_ubo);

	//create pipeline
	//update descriptor for material

	update_descriptor_set(resources.terrain_descriptor, terrain_descriptor);

	for (uint i = 0; i < RenderPass::ScenePassCount; i++) {
		PipelineDesc pipeline_desc = {};
		pipeline_desc.shader = resources.terrain_shader;
		//pipeline_desc.render_pass = &render_pass[i];
	}

	//MaterialDesc mat{ terrain_shader };
	//if (!(render_ctx.layermask & SHADOW_LAYER)) {
	//	mat.params = material_desc(materials->materials[0])->params;
	//}

	//MaterialDesc mat{ resources.terrain_shader };
	//mat.params = material_desc(mat_handle)->params;
	//mat_image(mat, "displacement", self->heightmap);
	//mat_float(mat, "max_height", self->max_height);
	//mat_vec2(mat, "displacement_scale", glm::vec2(1.0 / self->width, 1.0 / self->height));
}

uint lod_from_dist(float dist) {
	if (dist < 50) return 0;
	else if (dist < 100) return 1;
	else return 2;
}

void extract_render_data_terrain(TerrainRenderData& render_data, World& world, const RenderPass render_passes[RenderPass::ScenePassCount], Layermask layermask) {
	glm::vec4 planes[RenderPass::ScenePassCount][6];
	glm::vec3 cam_pos[RenderPass::ScenePassCount];
	
	for (int i = 0; i < RenderPass::ScenePassCount; i++) {
		extract_planes(render_passes[i].viewport, planes[i]);
		cam_pos[i] = glm::vec3(render_passes[i].viewport.view[3]);		
	}
	
	//TODO THIS ASSUMES EITHER 0 or 1 TERRAINS
	for (ID id : world.filter<Terrain, Transform, Materials>(layermask)) { //todo heavily optimize terrain
		auto self = world.by_id<Terrain>(id);
		auto self_trans = world.by_id<Transform>(id);
		auto materials = world.by_id<Materials>(id);

		for (uint w = 0; w < self->width; w++) {
			for (uint h = 0; h < self->height; h++) {
				//Calculate position of chunk
				Transform t;
				t.position = self_trans->position + glm::vec3(w * self->size_of_block, 0, (h + 1) * self->size_of_block);
				t.scale = glm::vec3((float)self->size_of_block);

				ChunkInfo chunk_info;
				chunk_info.model_m = compute_model_matrix(t);
				chunk_info.displacement_offset = glm::vec2(1.0 / self->width * w, 1.0 / self->height * h);

				//Cull and compute lod of chunk
				AABB aabb;
				aabb.min = glm::vec3(0, 0, -(int)self->size_of_block) + t.position;
				aabb.max = glm::vec3(self->size_of_block, self->max_height, 0) + t.position;

				for (uint pass = 0; pass < RenderPass::ScenePassCount; pass++) {
					if (frustum_test(planes[pass], aabb) == OUTSIDE) continue;

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

void render_terrain(TerrainRenderResources& resources, TerrainRenderData& data, RenderPass render_passes[RenderPass::ScenePassCount], Layermask layermask) {
	for (uint pass = 0; pass < RenderPass::ScenePassCount; pass++) {
		CommandBuffer& cmd_buffer = render_passes[pass].cmd_buffer;

		bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_TERRAIN_CHUNK);
		bind_descriptor(cmd_buffer, 1, resources.terrain_descriptor);

		for (uint i = 0; i < MAX_TERRAIN_CHUNK_LOD; i++) { //todo heavily optimize terrain
			tvector<ChunkInfo>& chunk_info = data.lod_chunks[pass][i];
			
			VertexBuffer vertex_buffer = get_VertexBuffer(resources.subdivided_plane[i], 0);
			InstanceBuffer instance_buffer = frame_alloc_instance_buffer<ChunkInfo>(INSTANCE_LAYOUT_TERRAIN_CHUNK, chunk_info);

			draw_mesh(cmd_buffer, vertex_buffer, instance_buffer);
		}
	}
}



