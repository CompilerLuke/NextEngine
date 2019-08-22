#include "stdafx.h"
#include "editor/terrain.h"
#include "editor/editor.h"
#include <glad/glad.h>
#include "graphics/texture.h"
#include "graphics/rhi.h"
#include "graphics/shader.h"
#include "model/model.h"
#include "logger/logger.h"
#include "components/transform.h"
#include "core/input.h"
#include <math.h>
#include "core/temporary.h"
#include "components/camera.h"

REFLECT_STRUCT_BEGIN(TerrainControlPoint)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Terrain)
REFLECT_STRUCT_MEMBER(width)
REFLECT_STRUCT_MEMBER(height)
REFLECT_STRUCT_MEMBER(size_of_block)
REFLECT_STRUCT_MEMBER_TAG(heightmap_points, reflect::HideInInspectorTag)
REFLECT_STRUCT_MEMBER(show_control_points)
REFLECT_STRUCT_END()

Terrain::Terrain() {}

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

		terrain->heightmap = RHI::texture_manager.make({
			"Heightmap",
			texture_id
		});
	}
}

TerrainSystem::TerrainSystem(World& world, Editor* editor) {
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

void TerrainSystem::update(World& world, UpdateParams& params) {
	if (!params.layermask & editor_layer) return;

	for (ID id : world.filter<Terrain, Transform>(params.layermask | game_layer)) {
		auto self = world.by_id<Terrain>(id);
		auto self_trans = world.by_id<Transform>(id);
		auto width_quads = 32 * self->width;
		auto height_quads = 32 * self->height;

		if (params.input.key_pressed('I')) {
			ID id = world.make_ID();
			Entity* e = world.make<Entity>(id);
			e->layermask = editor_layer | picking_layer;

			Transform* trans = world.make<Transform>(id);
			trans->scale = glm::vec3(0.1);
			trans->position = editor->place_at_cursor(params.input);

			TerrainControlPoint* control = world.make<TerrainControlPoint>(id);

			EntityEditor* name = world.make<EntityEditor>(id);
			name->name = "Control Point";

			editor->select(id);
		}

		if (!params.input.key_pressed('B')) continue;

		auto texture_id = texture::id_of(self->heightmap);
		
		if (!self->show_control_points) continue;

		auto control_points_filtered = world.filter<TerrainControlPoint, Transform>(editor_layer);

		auto& heightmap = self->heightmap_points;
		heightmap.clear();

		heightmap.reserve(width_quads * height_quads);

		auto size_per_quad = self->size_of_block / 32.0f;

		for (unsigned int h = 0; h < height_quads; h++) {
			for (unsigned int w = 0; w < width_quads; w++) {
				float height = 0.0f;
				float total_weight = 0.1f;

				for (ID id : control_points_filtered) {
					auto control_point = world.by_id<TerrainControlPoint>(id);
					auto control_point_trans = world.by_id<Transform>(id);

					auto pos = glm::vec2(w, h);
					auto p = glm::vec2(control_point_trans->position.x - self_trans->position.x, control_point_trans->position.z - self_trans->position.z);
					p /= size_per_quad;

					float radius = 50.0f;
					float dist = glm::length(p - pos);

					float weight = std::powf(glm::max(radius - dist, 0.0f) / radius, 1.3f);
					total_weight += weight;

					height += weight * (control_point_trans->position.y);				
				}

				height = height / total_weight / self->max_height;
				heightmap.append(height);
			}
		}
	
		glBindTexture(GL_TEXTURE_2D, texture_id);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_quads, height_quads, GL_RED, GL_FLOAT, heightmap.data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
}

void TerrainSystem::render(World& world, RenderParams& render_params) {
	Camera* cam = get_camera(world, render_params.layermask);
	Transform* cam_trans = world.by_id<Transform>(world.id_of(cam));
	
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



