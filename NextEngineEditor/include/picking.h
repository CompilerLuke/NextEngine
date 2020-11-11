#pragma once

#include "engine/handle.h"
#include "graphics/assets/material.h"
#include "graphics/culling/culling.h"
#include "core/container/slice.h"
#include "ecs/id.h"
#include <limits.h>

struct World;
struct RenderPass;
struct ShaderConfig;
struct Input;
struct Camera;
struct Assets;

const uint GIZMO_TAG = 1 << 0;
const uint MAX_PICKING_NODES = 100;
const uint MAX_PICKING_INSTANCES = 1000;

struct PickingScenePartition : Partition {
	AABB aabbs[MAX_PICKING_INSTANCES];
	ID ids[MAX_PICKING_INSTANCES];
	uint tags[MAX_PICKING_INSTANCES];
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
	PickingScenePartition partition;

	PickingSystem();
	void rebuild_acceleration_structure(World& world);
	bool ray_cast(const Ray&, RayHit&);
	bool ray_cast(Viewport&, glm::vec2 position, RayHit& hit);
	void visualize(Viewport&, glm::vec2, RenderPass& ctx);
	int pick(Viewport&, glm::vec2);
};

//void build_acceleration_structure(PickingScenePartition& scene_partition, World& world);
//bool ray_cast(PickingScenePartition& scene_partition, const Ray&, RayHit&);
//bool ray_cast(PickingScenePartition& scene_partition, World& world, Input& input, RayHit& hit);
//void visualize(PickingScenePartition& scene_parition, World& world, Input& input, RenderPass& ctx);
//int pick(PickingScenePartition&, World& world, Input&);

struct OutlineRenderState {
	shader_handle outline_shader;
	material_handle outline_material;
	material_handle object_material;
	DrawCommandState object_state;
	DrawCommandState outline_state;

	//OutlineSelected(Assets&);
	//void render_outlines(struct World&, slice<ID> highlighted, struct RenderPass&);
};

void make_outline_render_state(OutlineRenderState& outline);
void render_object_selected_outline(OutlineRenderState& outline, World&, slice<ID> highlighted, RenderPass&);
