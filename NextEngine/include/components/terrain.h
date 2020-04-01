#include "core/core.h"
#include "core/reflection.h"

struct TerrainControlPoint {
	REFLECT(ENGINE_API)
};

struct Terrain {
	uint width = 12;
	uint height = 12;
	uint size_of_block = 10;
	bool show_control_points;
	texture_handle heightmap;

	vector<float> heightmap_points;
	float max_height = 10.0f;

	REFLECT(ENGINE_API)
};

float ENGINE_API barryCentric(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 pos);
float ENGINE_API sample_terrain_height(Terrain* terrain, struct Transform* terrain_trans, glm::vec2 position);