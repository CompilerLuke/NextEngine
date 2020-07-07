#include "components/terrain.h"
#include "components/transform.h"
#include "core/reflection.h"

float barryCentric(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 pos) {
	float det = (p2.z - p3.z) * (p1.x - p3.x) + (p3.x - p2.x) * (p1.z - p3.z);
	float l1 = ((p2.z - p3.z) * (pos.x - p3.x) + (p3.x - p2.x) * (pos.y - p3.z)) / det;
	float l2 = ((p3.z - p1.z) * (pos.x - p3.x) + (p1.x - p3.x) * (pos.y - p3.z)) / det;
	float l3 = 1.0f - l1 - l2;
	return l1 * p1.y + l2 * p2.y + l3 * p3.y;
}

float sample_terrain_height(Terrain* terrain, Transform* terrain_trans, glm::vec2 position) {
	glm::vec2 quad_size = glm::vec2(terrain->size_of_block / 32.0f);

	glm::vec2 sample = position - glm::vec2(terrain_trans->position.x, terrain_trans->position.z);
	glm::vec2 local = sample;
	sample /= quad_size;

	int gridX = (int)sample.x;
	int gridZ = (int)sample.y;

	int width = 32 * terrain->width;

	float xCoord = fmodf(local.x, quad_size.x) / quad_size.x;
	float zCoord = fmodf(local.y, quad_size.y) / quad_size.y;

	if (xCoord <= (1 - zCoord)) {
		return terrain->max_height * barryCentric(glm::vec3(0, terrain->heightmap_points[gridX + gridZ * width], 0), glm::vec3(1,
			terrain->heightmap_points[gridX + 1 + gridZ * width], 0), glm::vec3(0,
				terrain->heightmap_points[gridX + (gridZ + 1) * width], 1), glm::vec2(xCoord, zCoord));
	}
	else {
		return terrain->max_height * barryCentric(glm::vec3(1, terrain->heightmap_points[gridX + 1 + gridZ * width], 0), glm::vec3(1,
			terrain->heightmap_points[gridX + 1 + (gridZ + 1) * width], 1), glm::vec3(0,
				terrain->heightmap_points[gridX + (gridZ + 1) * width], 1), glm::vec2(xCoord, zCoord));
	}
}
