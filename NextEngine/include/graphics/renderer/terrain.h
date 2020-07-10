#pragma once

#include "core/handle.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/rhi/buffer.h"
#include "graphics/rhi/shader_access.h"
#include "graphics/pass/pass.h"
#include "ecs/layermask.h"
#include <glm/mat4x4.hpp>

struct Assets;
struct World;
struct RenderPass;

struct ChunkInfo {
	glm::mat4 model_m;
	glm::vec2 displacement_offset;
};

struct TerrainRenderResources {	
	shader_handle terrain_shader;
	shader_handle flat_shader;

	model_handle cube_model;
	model_handle subdivided_plane[3];
	material_handle control_point_material;

	UBOBuffer terrain_ubo;
	texture_handle blend_idx_map;
	texture_handle blend_values_map;
	texture_handle displacement_map;
	sampler_handle blend_idx_sampler;
	sampler_handle blend_values_sampler;
	sampler_handle displacement_sampler;
	pipeline_handle terrain_pipeline[RenderPass::ScenePassCount];
	descriptor_set_handle terrain_descriptor;
};

struct TerrainUBO {
	glm::vec2 displacement_scale;
	glm::vec2 transformUVs;
	alignas(16) float max_height;
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
ENGINE_API void update_terrain_material(TerrainRenderResources& resources, Terrain& terrain);
void extract_render_data_terrain(TerrainRenderData& render_data, World& world, const Viewport [RenderPass::ScenePassCount], Layermask layermask);
void render_terrain(TerrainRenderResources& resources, const TerrainRenderData& data, RenderPass render_passes[RenderPass::ScenePassCount]);

