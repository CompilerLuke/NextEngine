#include "core/reflection.h"
#include "core/handle.h"
#include "core/container/vector.h"
#include "graphics/renderer/renderer.h"
#include "components/transform.h"
#include "components/terrain.h"
#include "ecs/ecs.h"
#include "terrain.h"
#include "core/io/input.h"
#include "editor.h"
#include "core/container/string_buffer.h"
#include "graphics/assets/assets.h"

void edit_Terrain(Editor& editor, Assets& assets, World& world, UpdateCtx& params) {
	if (!(params.layermask & EDITOR_LAYER)) return;

	for (auto [e, trans, terrain] : world.filter<Transform, Terrain>(params.layermask | GAME_LAYER)) {
		auto width_quads = 32 * terrain.width;
		auto height_quads = 32 * terrain.height;

		if (params.input.key_pressed('I')) { 
			auto[e,trans,control] = world.make<Transform, TerrainControlPoint>();
			e.layermask = EDITOR_LAYER | PICKING_LAYER;

			trans.scale = glm::vec3(0.1);
			trans.position = editor.place_at_cursor();

			editor.create_new_object("Control Point", e.id);
		}

		if (!params.input.key_pressed('B')) continue;

		//auto texture_id = gl_id_of(assets.textures, self->heightmap);
		
		if (!terrain.show_control_points) continue;

		auto control_points_filtered = world.filter<TerrainControlPoint, Transform>(EDITOR_LAYER);

		auto& heightmap = terrain.displacement_map;
		heightmap.clear();

		heightmap.reserve(width_quads * height_quads);

		auto size_per_quad = terrain.size_of_block / 32.0f;

		glm::vec3 terrain_position = trans.position;

		//todo optimize this loop
		for (unsigned int h = 0; h < height_quads; h++) {
			for (unsigned int w = 0; w < width_quads; w++) {
				float height = 0.0f;
				float total_weight = 0.1f;

				for (auto [e,control_point,control_point_trans] : control_points_filtered) {
					auto pos = glm::vec2(w, h);
					auto p = glm::vec2(control_point_trans.position.x - terrain_position.x, control_point_trans.position.z - terrain_position.z);
					p /= size_per_quad;

					float radius = 50.0f;
					float dist = glm::length(p - pos);

					float weight = std::powf(glm::max(radius - dist, 0.0f) / radius, 1.3f);
					total_weight += weight;

					height += weight * (control_point_trans.position.y);				
				}

				height = height / total_weight / terrain.max_height;
				heightmap.append(height);
			}
		}
	
		//glBindTexture(GL_TEXTURE_2D, texture_id);
		//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_quads, height_quads, GL_RED, GL_FLOAT, heightmap.data);
		//glGenerateMipmap(GL_TEXTURE_2D);
	}
}

