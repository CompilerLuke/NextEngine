#include "reflection/reflection.h"

struct TerrainControlPoint {
	REFLECT(ENGINE_API)
};

struct Terrain {
	unsigned int width = 12;
	unsigned int height = 12;
	unsigned int size_of_block = 10;
	bool show_control_points;
	Handle<struct Texture> heightmap;

	vector<float> heightmap_points;
	float max_height = 10.0f;

	REFLECT(ENGINE_API)
};

float ENGINE_API barryCentric(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 pos);
float ENGINE_API sample_terrain_height(Terrain* terrain, struct Transform* terrain_trans, glm::vec2 position);