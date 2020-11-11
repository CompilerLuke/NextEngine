#pragma once

#include "engine/handle.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/rhi/buffer.h"
#include "graphics/rhi/shader_access.h"
#include "graphics/pass/pass.h"
#include "terrain/kriging.h"
#include <glm/mat4x4.hpp>
#include "graphics/assets/material.h"

//#include "graphics/rhi/vulkan/vulkan.h"
//#include "graphics/rhi/vulkan/draw.h"

//#include "graphics/rhi/vulkan/async_cpu_copy.h"

#include "ecs/id.h"

struct Assets;
struct World;
struct RenderPass;

struct ChunkInfo {
	glm::mat4 model_m;
	glm::vec2 displacement_offset;
	float lod;
	float edge_lod;
};

struct TerrainRenderResources {	
	shader_handle terrain_shader;
	model_handle subdivided_plane[3];

	UBOBuffer terrain_ubo;
	texture_handle blend_idx_map;
	texture_handle blend_values_map;
	texture_handle displacement_map;
	sampler_handle blend_idx_sampler;
	sampler_handle blend_values_sampler;
	sampler_handle displacement_sampler;
	pipeline_handle color_terrain_pipeline;
	pipeline_handle depth_terrain_pipeline;
	periodically_updated_descriptor terrain_descriptor;

	UBOBuffer kriging_ubo;
	pipeline_handle kriging_pipeline;
	descriptor_set_handle kriging_descriptor;

	UBOBuffer splat_ubo;
	pipeline_handle splat_pipeline;
	descriptor_set_handle splat_descriptor;

	struct AsyncCopyResources* async_copy;
};

struct TerrainUBO {
	glm::vec2 displacement_scale;
	glm::vec2 transformUVs;
	float max_height;
	float grid_size;
};

#define MAX_TERRAIN_CHUNK_LOD 3

struct CulledTerrainChunk {
	tvector<ChunkInfo> chunks[MAX_TERRAIN_CHUNK_LOD];
};

struct TerrainRenderData {
	TerrainUBO terrain_ubo;
	tvector<ChunkInfo> lod_chunks[RenderPass::ScenePassCount][MAX_TERRAIN_CHUNK_LOD];
};

struct SplatPushConstant {
	glm::vec2 brush_offset;
	float radius;
	float hardness;
	float min_height;
	float max_height;
	int material;
};

struct Terrain;

void init_terrain_render_resources(TerrainRenderResources&);
ENGINE_API void default_terrain(Terrain& terrain);
ENGINE_API void default_terrain_material(Terrain& terrain);
ENGINE_API void update_terrain_material(TerrainRenderResources& resources, Terrain& terrain);
void extract_render_data_terrain(TerrainRenderData& render_data, World& world, const Viewport [RenderPass::ScenePassCount], EntityQuery layermask);
void render_terrain(TerrainRenderResources& resources, const TerrainRenderData& data, RenderPass render_passes[RenderPass::ScenePassCount]);

ENGINE_API void clear_terrain(TerrainRenderResources& resources);
ENGINE_API void gpu_estimate_terrain_surface(TerrainRenderResources& resources, KrigingUBO& ubo);
ENGINE_API void gpu_splat_terrain(TerrainRenderResources& resources, slice<glm::mat4> models, slice<SplatPushConstant> splats);
ENGINE_API void regenerate_terrain(World& world, TerrainRenderResources& resources, EntityQuery layermask);
ENGINE_API void receive_generated_heightmap(TerrainRenderResources& resources, Terrain& terrain);
