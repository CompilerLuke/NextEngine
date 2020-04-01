#include "stdafx.h"
#include "graphics/renderer/terrain.h"
#include "graphics/assets/model.h"
#include "graphics/assets/texture.h"
#include "graphics/assets/shader.h"
#include "core/io/logger.h"
#include "components/transform.h"
#include "components/terrain.h"
#include "components/camera.h"
#include "graphics/renderer/material_system.h"
#include "graphics/renderer/renderer.h"
#include "graphics/rhi/rhi.h"

void init_terrains(TextureManager& texture_manager, World& world, vector<ID>& terrains) {
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

		terrain->heightmap = texture_manager.create_from(image, tex_desc);

		image.data = NULL; //todo image should not assume, pointer is from stbi
	}
}

model_handle load_subdivided(ModelManager& model_manager, uint num) {
	return model_manager.load(tformat("subdivided_plane", num, ".fbx"));
}

TerrainRenderSystem::TerrainRenderSystem(AssetManager& assets, World& world) : asset_manager(assets) {
	world.on_make<Terrain>([&assets, &world](vector<ID>& terrains) { init_terrains(assets.textures, world, terrains); });

	flat_shader = assets.shaders.load("shaders/pbr.vert", "shaders/gizmo.frag");
	terrain_shader = assets.shaders.load("shaders/terrain.vert", "shaders/terrain.frag");

	subdivided_plane[0] = load_subdivided(assets.models, 32);
	subdivided_plane[1] = load_subdivided(assets.models, 16);
	subdivided_plane[2] = load_subdivided(assets.models, 8);
	cube_model = assets.models.load("cube.fbx");

	Material mat(flat_shader);
	mat.set_vec3(assets.shaders, "color", glm::vec3(1, 1, 0));

	control_point_materials.append(assets.materials.assign_handle(std::move(mat)));

	//vector<VertexAttrib> chunk_info_layout = INSTANCE_MAT4X4_LAYOUT;
	//chunk_info_layout.append({2, Float, offsetof(ChunkInfo, displacement_offset)});
	//chunk_info_layout.append({1, Int, offsetof(ChunkInfo, lod)});

	for (int i = 0; i < 3; i++) {
		chunk_instance_buffer[i] = RHI::alloc_instance_buffer(VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_TERRAIN_CHUNK, 144, NULL);
	}
}

void TerrainRenderSystem::render(World& world, RenderCtx& render_ctx) {
	ShaderManager& shaders = asset_manager.shaders;
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

		Material* mat = TEMPORARY_ALLOC(Material);
		mat->shader = terrain_shader;
		mat->params.allocator = &temporary_allocator;
		if (!(render_ctx.layermask & SHADOW_LAYER)) {
			mat->retarget_from(asset_manager, materials->materials[0]);
		}
		mat->set_image(shaders, "displacement", self->heightmap);
		mat->set_float(shaders, "max_height", self->max_height);
		mat->set_vec2(shaders, "displacement_scale", glm::vec2(1.0 / self->width, 1.0 / self->height));

		vector<ChunkInfo> lod_chunks[3];
		lod_chunks[0].allocator = &temporary_allocator;
		lod_chunks[1].allocator = &temporary_allocator;
		lod_chunks[2].allocator = &temporary_allocator;

		for (unsigned int w = 0; w < self->width; w++) {
			for (unsigned int h = 0; h < self->height; h++) {
				Transform t;
				t.position = self_trans->position + glm::vec3(w * self->size_of_block, 0, (h + 1.0) * self->size_of_block);
				t.scale = glm::vec3(self->size_of_block);

				ChunkInfo chunk_info;
				chunk_info.model_m = t.compute_model_matrix();

				AABB aabb;
				aabb.min = glm::vec3(0, 0, 0) + t.position;
				aabb.max = glm::vec3(self->size_of_block, self->max_height, self->size_of_block) + t.position;

				//if (frustum_test(planes, aabb) == OUTSIDE) continue;

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
			
			RHI::upload_data(chunk_instance_buffer[i], chunk_info);

			Model* model = asset_manager.models.get(subdivided_plane[i]);

			cmd_buffer.draw(chunk_info.length, &model->meshes[0].buffer, &chunk_instance_buffer[i], mat);
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



