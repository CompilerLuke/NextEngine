#pragma once

#include "model/model.h"
#include "graphics/rhi.h"

struct Instance {
	vector<glm::mat4> transforms;
	vector<AABB> aabbs;
	vector<ID> ids;
	VertexBuffer* buffer;
};

struct ModelRendererSystem : System {
	static constexpr unsigned MAX_VAO = 30;
	static constexpr unsigned int NUM_INSTANCES = MAX_VAO * RHI::material_manager.MAX_HANDLES;
	Instance instances[NUM_INSTANCES];

	//one possibility is splitting dynamic and static meshes

	void pre_render(World&, PreRenderParams&) override;
	void render(World&, RenderParams&) override;
};