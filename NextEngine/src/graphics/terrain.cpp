#include "stdafx.h"
#include "graphics/terrain.h"
#include "model/model.h"
#include "components/terrain.h"
#include "graphics/shader.h"
#include "graphics/rhi.h"
#include "logger/logger.h"
#include "components/transform.h"
#include "graphics/texture.h"
#include "graphics/materialSystem.h"
#include "components/camera.h"
#include "graphics/renderer.h"

Handle<Model> load_subdivided(unsigned int num) {
	return load_Model(format("subdivided_plane", num, ".fbx"));
}

void init_terrains(World& world, vector<ID>& terrains) {
	for (ID id : terrains) {
		auto terrain = world.by_id<Terrain>(id);
		if (!terrain) continue;

		unsigned int texture_id;

		glGenTextures(1, &texture_id);
		glBindTexture(GL_TEXTURE_2D, texture_id);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16); 

		unsigned int width_quads = 32 * terrain->width;
		unsigned int height_quads = 32 * terrain->height;

		if (terrain->heightmap_points.length == 0) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width_quads, height_quads, 0, GL_RED, GL_FLOAT, NULL);
		}
		else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width_quads, height_quads, 0, GL_RED, GL_FLOAT, terrain->heightmap_points.data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		glBindTexture(GL_TEXTURE_2D, 0);

		terrain->heightmap = RHI::texture_manager.make(Texture{
			"Heightmap",
			texture_id
			});
	}
}

TerrainRenderSystem::TerrainRenderSystem(World& world, Editor* editor) {
	world.on_make<Terrain>([this, &world](vector<ID>& terrains) { init_terrains(world, terrains); });

	flat_shader = load_Shader("shaders/pbr.vert", "shaders/gizmo.frag");
	terrain_shader = load_Shader("shaders/terrain.vert", "shaders/terrain.frag");

	subdivided_plane32 = load_subdivided(32);
	subdivided_plane16 = load_subdivided(16);
	subdivided_plane8 = load_subdivided(8);
	cube_model = load_Model("cube.fbx");

	this->editor = editor;

	Material mat(flat_shader);
	mat.set_vec3("color", glm::vec3(1, 1, 0));

	control_point_materials.append(RHI::material_manager.make(std::move(mat)));
}

void TerrainRenderSystem::render(World& world, RenderParams& render_params) {
	ID cam = get_camera(world, render_params.layermask);
	Transform* cam_trans = world.by_id<Transform>(cam);

	glm::vec4 planes[6];
	extract_planes(render_params, planes);

	int rendering_block = 0;

	for (ID id : world.filter<Terrain, Transform, Materials>(render_params.layermask)) {
		auto self = world.by_id<Terrain>(id);
		auto self_trans = world.by_id<Transform>(id);
		auto materials = world.by_id<Materials>(id);

		unsigned int lod0 = 0;
		unsigned int lod1 = 1;
		unsigned int lod2 = 2;

		for (unsigned int w = 0; w < self->width; w++) {
			for (unsigned int h = 0; h < self->width; h++) {
				Transform t;
				t.position = self_trans->position + glm::vec3(w * self->size_of_block, 0, (h + 1.0) * self->size_of_block);
				t.scale = glm::vec3(self->size_of_block);

				glm::mat4* model_m = TEMPORARY_ALLOC(glm::mat4);
				*model_m = t.compute_model_matrix();

				AABB aabb = RHI::model_manager.get(this->subdivided_plane8)->aabb;
				aabb.max.y = 1.0f;
				aabb = aabb.apply(*model_m);

				if (cull(planes, aabb)) continue;

				Material* mat = TEMPORARY_ALLOC(Material);
				mat->shader = terrain_shader;
				mat->params.allocator = &temporary_allocator;

				if (!(render_params.layermask & shadow_layer)) {
					mat->retarget_from(materials->materials[0]);
				}

				mat->set_image("displacement", self->heightmap);
				mat->set_vec2("displacement_offset", glm::vec2(1.0 / self->width * w, 1.0 / self->height * h));
				mat->set_vec2("displacement_scale", glm::vec2(1.0 / self->width, 1.0 / self->height));
				mat->set_float("max_height", self->max_height);

				int lod = 0;

				if (render_params.layermask & shadow_layer) {
					lod = 2;
				}
				else {
					float dist = glm::length(t.position - cam_trans->position);

					if (dist < 50) lod = 0;
					else if (dist < 100) lod = 1;
					else lod = 2;
				}

				DrawCommand cmd(id, model_m, NULL, mat);

				if (lod == 0) {
					cmd.buffer = &RHI::model_manager.get(subdivided_plane32)->meshes[0].buffer;
				}
				else if (lod == 1) {
					cmd.buffer = &RHI::model_manager.get(subdivided_plane16)->meshes[0].buffer;
				}
				else {
					cmd.buffer = &RHI::model_manager.get(subdivided_plane8)->meshes[0].buffer;
				}
				mat->set_int("lod", lod);

				rendering_block++;


				render_params.command_buffer->submit(cmd);
			}
		}

		if (self->show_control_points && (render_params.layermask & editor_layer || render_params.layermask && picking_layer)) {
			for (ID id : world.filter<TerrainControlPoint, Transform>(render_params.layermask)) {
				Transform* trans = world.by_id<Transform>(id);
				trans->scale = glm::vec3(0.1);

				world.by_id<Entity>(id)->layermask |= picking_layer;

				auto model_m = TEMPORARY_ALLOC(glm::mat4);
				*model_m = trans->compute_model_matrix();
				RHI::model_manager.get(cube_model)->render(id, model_m, control_point_materials, render_params);
			}
		}
	}
}



