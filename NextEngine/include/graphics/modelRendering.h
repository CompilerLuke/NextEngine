#pragma once

#include "model/model.h"
#include "graphics/renderFeature.h"

struct Instance {
	vector<glm::mat4> transforms;
	vector<AABB> aabbs;
	vector<ID> ids;
	VertexBuffer* buffer;
};

struct ModelRendererSystem : RenderFeature {
	static constexpr unsigned MAX_VAO = 30;
	static constexpr unsigned int NUM_INSTANCES = MAX_VAO * 200;
	Instance instances[NUM_INSTANCES];

	//one possibility is splitting dynamic and static meshes

	void pre_render(World&, PreRenderParams&) override;
	void render(World&, RenderParams&) override;
};