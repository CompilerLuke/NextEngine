#include "stdafx.h"
#include "reflection/reflection.h"
#include "core/handle.h"
#include "core/vector.h"
#include "graphics/renderer.h"
#include "components/transform.h"
#include "components/terrain.h"
#include "ecs/ecs.h"
#include "terrain.h"
#include <glad/glad.h>
#include "core/input.h"
#include "editor.h"
#include "core/string_buffer.h"
#include "graphics/texture.h"

void edit_Terrain(Editor& editor, World& world, UpdateParams& params) {
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
			trans->position = editor.place_at_cursor();

			TerrainControlPoint* control = world.make<TerrainControlPoint>(id);

			EntityEditor* name = world.make<EntityEditor>(id);
			name->name = "Control Point";

			editor.select(id);
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

