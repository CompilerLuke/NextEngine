#include "core/container/vector.h"
#include "components/transform.h"
#include "components/terrain.h"
#include "ecs/ecs.h"
#include "terrain_tools/terrain.h"
#include "engine/input.h"
#include "editor.h"

struct TerrainSplatWeight {
	glm::vec2 position;
	uint material;
};

float ease_in_out_cubic(float x) {
	return x < 0.5 ? 4 * x * x * x : 1 - powf(-2 * x + 2, 3) / 2;
}

void edit_Terrain(Editor& editor, World& world, UpdateCtx& params) {
	if (params.input.key_pressed(Key::I)) {
		auto[e, trans, control] = world.make<Transform, TerrainControlPoint>(EDITOR_ONLY);

		trans.scale = glm::vec3(0.1);
		trans.position = editor.place_at_cursor();

		editor.create_new_object("Control Point", e.id);
	}

	if (params.input.key_pressed(Key::O)) {
		auto[e, trans, control] = world.make<Transform, TerrainSplat>(EDITOR_ONLY);

		trans.scale = glm::vec3(1.0);
		trans.position = editor.place_at_cursor();

		editor.create_new_object("Splat", e.id);
	}

	auto terrains = world.first<Transform, Terrain>();
	if (!terrains) return;

	auto[_, terrain_trans, terrain] = *terrains;

	for (auto[e, trans, splat] : world.filter<Transform, TerrainSplat>({ EDITOR_ONLY })) {
		trans.position.y = sample_terrain_height(terrain, terrain_trans, glm::vec2(trans.position.x, trans.position.z));
	}
}

