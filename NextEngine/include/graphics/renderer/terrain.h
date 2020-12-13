#pragma once

#include "ecs/id.h"
#include "engine/handle.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/rhi/buffer.h"
#include "graphics/rhi/shader_access.h"
#include "graphics/pass/pass.h"
#include <glm/mat4x4.hpp>
#include "graphics/assets/material.h"

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

	sampler_handle blend_values_sampler;
	sampler_handle blend_idx_sampler;
	sampler_handle displacement_sampler;

	pipeline_handle color_terrain_pipeline;
	pipeline_handle depth_terrain_prepass_pipeline;
	pipeline_handle depth_terrain_pipeline;

	pipeline_handle kriging_pipeline;
	descriptor_set_handle kriging_descriptor;

	UBOBuffer ubo;
	texture_handle blend_idx_map;
	texture_handle blend_values_map;
	texture_handle displacement_map;
	periodically_updated_descriptor descriptor;
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

struct Terrain;

void init_terrain_render_resources(TerrainRenderResources&);
ENGINE_API void default_terrain(Terrain& terrain);
ENGINE_API void default_terrain_material(Terrain& terrain);
ENGINE_API void update_terrain_material(TerrainRenderResources& resources, Terrain& terrain);
void extract_render_data_terrain(TerrainRenderData& render_data, World& world, const Viewport [RenderPass::ScenePassCount], EntityQuery layermask);
void render_terrain(TerrainRenderResources& resources, const TerrainRenderData& data, RenderPass render_passes[RenderPass::ScenePassCount]);

ENGINE_API void clear_terrain(TerrainRenderResources& resources);
