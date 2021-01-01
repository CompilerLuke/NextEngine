#pragma once

#include "engine/handle.h"
#include "ecs/id.h"
#include "core/container/slice.h"
#include "graphics/rhi/buffer.h"
#include <glm/mat4x4.hpp>

struct KrigingUBO;
struct SplatPushConstant;
struct World;
struct Terrain;
struct TerrainRenderResources;

struct TerrainEditorResources {
	UBOBuffer kriging_ubo;
	pipeline_handle kriging_pipeline;
	descriptor_set_handle kriging_descriptor;

	UBOBuffer splat_ubo;
	pipeline_handle splat_pipeline;
	descriptor_set_handle splat_descriptor;

	struct AsyncCopyResources* async_copy = nullptr;
};

struct SplatPushConstant {
	glm::vec2 brush_offset;
	float radius;
	float hardness;
	float min_height;
	float max_height;
	int material;
};

void init_terrain_editor_render_resources(TerrainEditorResources& resources, TerrainRenderResources& terrain_resources, Terrain& terrain);
void gpu_estimate_terrain_surface(TerrainEditorResources& generation_resources, TerrainRenderResources& render_resources, KrigingUBO& ubo);
void gpu_splat_terrain(TerrainEditorResources& resources, slice<glm::mat4> models, slice<SplatPushConstant> splats);
void regenerate_terrain(World& world, TerrainEditorResources& generation_resources, TerrainRenderResources& terrain_resources, EntityQuery layermask);
void receive_generated_heightmap(TerrainEditorResources& resources, Terrain& terrain);