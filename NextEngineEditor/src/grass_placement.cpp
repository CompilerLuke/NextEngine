#include "ecs/ecs.h"
#include "grass.h"
#include "components/terrain.h"
#include "graphics/renderer/grass.h"
#include "core/event.h"

void place_Grass(World& world, ID id) {
	auto terrains = world.first<Transform, Terrain>(); //todo move this code into ecs.h
	if (!terrains) return;
	
	auto [e,terrain_transform, terrain] = *terrains;

	auto [transform, grass] = *world.get_by_id<Transform, Grass>(id);

	glm::vec2 step = (1.0f / grass.density) / glm::vec2(grass.width, grass.height);
	grass.transforms.clear();

	for (float a = -.5f * grass.width; a < .5f * grass.width; a += step.x) {
		for (float b = -.5f * grass.height; b < .5f * grass.height; b += step.x) {
			float x = step.x * ((float)rand() / RAND_MAX);
			float y = step.y * ((float)rand() / RAND_MAX);

			Transform trans = {
				glm::vec3(a + x, 0, b + y),
				glm::angleAxis(glm::radians(grass.random_rotation * (rand() % 360)), glm::vec3(0, 1.0f,0)),
				glm::vec3(1)
			};

			trans.position += transform.position;
			trans.position.y = sample_terrain_height(terrain, terrain_transform, glm::vec2(trans.position.x, trans.position.z));
			
			if (trans.position.y > grass.max_height) continue;
			grass.transforms.append(trans);
		}
	}


	//ComponentEdit edit{ EDIT_GRASS_PLACEMENT };
	//edit.id = world.id_of(grass);



	//GrassRenderSystem::update_buffers(world, world.id_of(grass));
}