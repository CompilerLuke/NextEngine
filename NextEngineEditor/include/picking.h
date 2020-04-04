#pragma once

#include "core/handle.h"
#include "graphics/renderer/material_system.h"
#include "graphics/culling/culling.h"
#include "core/container/slice.h"
#include <limits.h>

struct World;
struct RenderCtx;
struct ShaderConfig;
struct Input;
struct Camera;
struct AssetManager;

#define MAX_PICKING_NODES 100
#define MAX_PICKING_INSTANCES 1000

struct PickingScenePartition : Partition {
	AABB aabbs[MAX_PICKING_INSTANCES];
	ID ids[MAX_PICKING_INSTANCES];
};

struct Ray {
	glm::vec3 orig, dir;
	glm::vec3 invdir;
	float t;

	int sign[3];

	inline Ray(glm::vec3 o, glm::vec3 d, float t = FLT_MAX) : orig(o), dir(d), t(t) {
		invdir = 1.0f / d;
		sign[0] = invdir.x < 0;
		sign[1] = invdir.y < 0;
		sign[2] = invdir.z < 0;
	}
};

struct RayHit {
	float t = FLT_MAX;
	glm::vec3 position;
	ID id;
};

struct PickingSystem {
	AssetManager& asset_manager;
	PickingScenePartition partition;

	PickingSystem(AssetManager&);
	void rebuild_acceleration_structure(World& world);
	bool ray_cast(const Ray&, RayHit&);
	bool ray_cast(World& world, Input& input, RayHit& hit);
	void visualize(World& world, Input& input, RenderCtx& ctx);
	int pick(World& world, Input&);
};

struct OutlineSelected {
	AssetManager& asset_manager;
	shader_handle outline_shader;
	Material outline_material;
	DrawCommandState object_state;
	DrawCommandState outline_state;

	OutlineSelected(AssetManager&);
	void render(struct World&, slice<ID> highlighted, struct RenderCtx&);
};