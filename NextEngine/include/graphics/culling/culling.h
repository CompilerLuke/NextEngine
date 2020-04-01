#pragma once

#include "scene_partition.h"
#include "aabb.h"
#include "graphics/pass/pass.h"

enum CullResult { INTERSECT, INSIDE, OUTSIDE };

void ENGINE_API extract_planes(struct RenderCtx&, glm::vec4 planes[6]);
CullResult ENGINE_API frustum_test(glm::vec4 planes[6], const AABB& aabb);

struct AssetManager;
struct ModelRendererSystem;

struct CullingSystem {
	AssetManager& asset_manager;
	ModelRendererSystem& model_renderer;
	ScenePartition scene_partition;

	CullingSystem(AssetManager& model_manager, ModelRendererSystem&, World& world);
	void cull(World& world, struct RenderCtx&);
	void build_acceleration(World& world);
	void render_debug_bvh(World& world, struct RenderCtx&);
};