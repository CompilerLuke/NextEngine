#include "reflection/reflection.h"
#include "core/handle.h"
#include "core/vector.h"
#include "ecs/system.h"

struct TerrainControlPoint {
	REFLECT()
};

struct Terrain {
	unsigned int width = 12;
	unsigned int height = 12;
	unsigned int size_of_block = 10;
	bool show_control_points;
	Handle<struct Texture> heightmap;
	
	vector<float> heightmap_points;
	float max_height = 10.0f;

	Terrain();

	REFLECT()
};

struct TerrainSystem : System {
	struct Editor* editor = NULL; 

	Handle<struct Shader> terrain_shader;
	Handle<struct Shader> flat_shader;

	Handle<struct Model> cube_model;
	Handle<struct Model> subdivided_plane8;
	Handle<struct Model> subdivided_plane16;
	Handle<struct Model> subdivided_plane32;
	vector<Handle<struct Material>> control_point_materials;

	TerrainSystem(struct World&, struct Editor* editor);

	void update(struct World&, struct UpdateParams&) override;
	void render(struct World&, struct RenderParams&) override;
};