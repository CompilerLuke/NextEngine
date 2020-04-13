#pragma once

#include "core/handle.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/rhi/buffer.h"
#include <glm/mat4x4.hpp>

struct Assets;
struct World;
struct RenderCtx;

struct ChunkInfo {
	glm::mat4 model_m;
	glm::vec2 displacement_offset;
};

struct TerrainRenderSystem : RenderFeature {
	RHI& rhi;
	Assets& assets;
	
	shader_handle terrain_shader;
	shader_handle flat_shader;

	model_handle cube_model;
	model_handle subdivided_plane[3];
	vector<material_handle> control_point_materials;

	InstanceBuffer chunk_instance_buffer[3];

	TerrainRenderSystem(RHI& rhi, Assets& assets, World&);

	void render(struct World&, struct RenderCtx&) override;
};

