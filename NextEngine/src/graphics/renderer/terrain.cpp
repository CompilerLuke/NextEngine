#include "stdafx.h"
#include "graphics/renderer/terrain.h"
#include "graphics/assets/assets.h"
#include "core/io/logger.h"
#include "components/transform.h"
#include "components/terrain.h"
#include "components/camera.h"
#include "graphics/assets/material.h"
#include "graphics/renderer/renderer.h"
#include "graphics/rhi/rhi.h"

void init_terrains(Assets& assets, World& world, vector<ID>& terrains) {
	for (ID id : terrains) {
		auto terrain = world.by_id<Terrain>(id);
		if (!terrain) continue;

		unsigned int texture_id;

		TextureDesc tex_desc;
		tex_desc.wrap_s = Wrap::ClampToBorder;
		tex_desc.wrap_t = Wrap::ClampToBorder;
		tex_desc.min_filter = Filter::LinearMipmapLinear;
		tex_desc.mag_filter = Filter::Linear;
		tex_desc.internal_format = InternalColorFormat::Red;
		tex_desc.external_format = ColorFormat::Red;
		tex_desc.texel_type = TexelType::Float;

		uint width_quads = 32 * terrain->width;
		uint height_quads = 32 * terrain->height;

		Image image;
		image.width = width_quads;
		image.height = height_quads;
		image.data = terrain->heightmap_points.length == 0 ? NULL : terrain->heightmap_points.data;

		//terrain->heightmap = upload_texture(assets, image, tex_desc);

		image.data = NULL; //todo image should not assume, pointer is from stbi
	}
}

model_handle load_subdivided(Assets& assets, uint num) {
	return load_Model(assets, tformat("subdivided_plane", num, ".fbx"));
}

TerrainRenderSystem::TerrainRenderSystem(RHI& rhi, Assets& assets, World& world) 
	: rhi(rhi), assets(assets) {
	world.on_make<Terrain>([&assets, &world](vector<ID>& terrains) { init_terrains(assets, world, terrains); });
	
	BufferAllocator& buffer_manager = get_BufferAllocator(rhi);

	flat_shader = load_Shader(assets, "shaders/pbr.vert", "shaders/gizmo.frag");
	terrain_shader = load_Shader(assets, "shaders/terrain.vert", "shaders/terrain.frag");

	subdivided_plane[0] = load_subdivided(assets, 32);
	subdivided_plane[1] = load_subdivided(assets, 16);
	subdivided_plane[2] = load_subdivided(assets, 8);
	cube_model = load_Model(assets, "cube.fbx");

	MaterialDesc mat_desc{ flat_shader };
	mat_vec3(mat_desc, "color", glm::vec3(1, 1, 0));

	control_point_materials.append(make_Material(assets, mat_desc));

	//vector<VertexAttrib> chunk_info_layout = INSTANCE_MAT4X4_LAYOUT;
	//chunk_info_layout.append({2, Float, offsetof(ChunkInfo, displacement_offset)});
	//chunk_info_layout.append({1, Int, offsetof(ChunkInfo, lod)});

	for (int i = 0; i < 3; i++) {
		chunk_instance_buffer[i] = alloc_instance_buffer(buffer_manager, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_TERRAIN_CHUNK, 144, NULL);
	}
}

struct TerrainUBO {
	alignas(16) float max_height;
	glm::vec2 displacement_scale;
};

void TerrainRenderSystem::render(World& world, RenderCtx& render_ctx) {
	BufferAllocator& buffer_allocator = get_BufferAllocator(rhi);
	CommandBuffer& cmd_buffer = render_ctx.command_buffer;

	ID cam = get_camera(world, render_ctx.layermask);
	Transform* cam_trans = world.by_id<Transform>(cam);

	glm::vec4 planes[6];
	extract_planes(render_ctx, planes);

	//TODO THIS ASSUMES EITHER 0 or 1 TERRAINS

	for (ID id : world.filter<Terrain, Transform, Materials>(render_ctx.layermask)) { //todo heavily optimize terrain
		auto self = world.by_id<Terrain>(id);
		auto self_trans = world.by_id<Transform>(id);
		auto materials = world.by_id<Materials>(id);

		TerrainUBO terrain_ubo = {};
		terrain_ubo.max_height = self->max_height;
		terrain_ubo.displacement_scale = glm::vec2(1.0 / self->width, 1.0 / self->height);

		MaterialDesc mat{terrain_shader};
		if (!(render_ctx.layermask & SHADOW_LAYER)) {
			mat.params = material_desc(assets, materials->materials[0])->params.copy();
		}			
		mat_image(mat, "displacement", self->heightmap);
		mat_float(mat, "max_height", self->max_height);
		mat_vec2(mat, "displacement_scale", glm::vec2(1.0 / self->width, 1.0 / self->height));

		vector<ChunkInfo> lod_chunks[3];
		lod_chunks[0].allocator = &temporary_allocator;
		lod_chunks[1].allocator = &temporary_allocator;
		lod_chunks[2].allocator = &temporary_allocator;

		for (unsigned int w = 0; w < self->width; w++) {
			for (unsigned int h = 0; h < self->height; h++) {
				Transform t;
				t.position = self_trans->position + glm::vec3(w * self->size_of_block, 0, (h + 1) * self->size_of_block);
				t.scale = glm::vec3((float)self->size_of_block);

				ChunkInfo chunk_info;
				chunk_info.model_m = t.compute_model_matrix();

				AABB aabb;
				aabb.min = glm::vec3(0, 0, -(int)self->size_of_block) + t.position;
				aabb.max = glm::vec3(self->size_of_block, self->max_height, 0) + t.position;

				if (frustum_test(planes, aabb) == OUTSIDE) continue;

				chunk_info.displacement_offset = glm::vec2(1.0 / self->width * w, 1.0 / self->height * h);

				int lod = 2;
				if (!(render_ctx.layermask & SHADOW_LAYER)) {
					float dist = glm::length(t.position - cam_trans->position);

					if (dist < 50) lod = 0;
					else if (dist < 100) lod = 1;
					else lod = 2;
				}

				lod_chunks[lod].append(chunk_info);
			}
		}

		int pass_id = render_ctx.pass->id;

		if (pass_id != 0) return;

		for (int i = 0; i < 3; i++) {
			vector<ChunkInfo>& chunk_info = lod_chunks[i];

			//upload_data(buffer_manager, chunk_instance_buffer[i], chunk_info);

			//Model* model = get_Model(assets, subdivided_plane[i]);

			//cmd_buffer.draw(chunk_info.length, &model->meshes[0].buffer, &chunk_instance_buffer[i], mat);
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
}



