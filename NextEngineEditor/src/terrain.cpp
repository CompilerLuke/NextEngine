#include "core/container/vector.h"
#include "components/transform.h"
#include "components/terrain.h"
#include "ecs/ecs.h"
#include "terrain.h"
#include "core/io/input.h"
#include "editor.h"

struct TerrainSplatWeight {
	glm::vec2 position;
	uint material;
};

float easeInOutCubic(float x) {
	return x < 0.5 ? 4 * x * x * x : 1 - powf(-2 * x + 2, 3) / 2;
}


void edit_Terrain(Editor& editor, World& world, UpdateCtx& params) {
	if (params.input.key_pressed('I')) {
		auto[e, trans, control] = world.make<Transform, TerrainControlPoint>();
		e.layermask = EDITOR_LAYER | PICKING_LAYER;

		trans.scale = glm::vec3(0.1);
		trans.position = editor.place_at_cursor();

		editor.create_new_object("Control Point", e.id);
	}

	if (params.input.key_pressed('O')) {
		auto[e, trans, control] = world.make<Transform, TerrainSplat>();
		e.layermask = EDITOR_LAYER | PICKING_LAYER;

		trans.scale = glm::vec3(1.0);
		trans.position = editor.place_at_cursor();

		editor.create_new_object("Splat", e.id);
	}

	auto terrains = world.first<Transform, Terrain>();
	if (!terrains) return;

	auto[_, terrain_trans, terrain] = *terrains;

	for (auto [e, trans, splat] : world.filter<Transform, TerrainSplat>(EDITOR_LAYER)) {
		trans.position.y = sample_terrain_height(terrain, terrain_trans, glm::vec2(trans.position.x, trans.position.z));
	}
}

