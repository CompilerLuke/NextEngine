#include "core/handle.h"
#include "renderFeature.h"

struct TerrainRenderSystem : RenderFeature {
	struct Editor* editor = NULL;

	Handle<struct Shader> terrain_shader;
	Handle<struct Shader> flat_shader;

	Handle<struct Model> cube_model;
	Handle<struct Model> subdivided_plane8;
	Handle<struct Model> subdivided_plane16;
	Handle<struct Model> subdivided_plane32;
	vector<Handle<struct Material>> control_point_materials;

	TerrainRenderSystem(struct World&, struct Editor* editor);

	void render(struct World&, struct RenderParams&) override;
};

