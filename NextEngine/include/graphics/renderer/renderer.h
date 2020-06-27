#pragma once

#include "core/handle.h"
#include "ecs/id.h"
#include "core/container/vector.h"
#include "core/container/handle_manager.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/pass/render_pass.h"
#include "graphics/renderer/terrain.h"
#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/lighting_system.h"
#include "graphics/renderer/ibl.h"
#include "graphics/culling/scene_partition.h"
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>

struct Modules;
struct Window;
struct World;
struct RenderPass;
struct ShaderConfig;
struct VertexStreaming;
struct RHI;

struct RenderSettings {
	enum GraphicsQuality { Ultra, High, Medium, Low } quality = Ultra;

	uint display_resolution_width, display_resolution_height;
	uint shadow_resolution;
};

struct Renderer;

struct FrameData {
	PassUBO pass_ubo;
	LightUBO light_ubo;
	SkyboxRenderData skybox_data;
	struct CulledMeshBucket* culled_mesh_bucket[RenderPass::ScenePassCount];
};

struct GPUSubmission {
	RenderPass render_passes[RenderPass::ScenePassCount];
	RenderPass screen_render_pass;
};

const uint SHADOW_CASCADES = 4;

struct ENGINE_API Renderer {
	Viewport viewport;
	RenderSettings settings;

	ScenePartition scene_partition;
	MeshBucketCache mesh_buckets;

	LightingSystem lighting_system;
	TerrainRenderResources terrain_render_resources;

	UBOBuffer scene_pass_ubo;
	descriptor_set_handle scene_pass_descriptor;

	//texture_handle shadow_fbo[4];
	pipeline_layout_handle pipeline_layout;

	texture_handle scene_map;
	texture_handle shadow_mask_map[SHADOW_CASCADES];
};

Renderer* make_Renderer(const RenderSettings&, World&);
void destroy_Renderer(Renderer*);

ENGINE_API void build_framegraph(Renderer& renderer, slice<Dependency>);
ENGINE_API void render_overlay(Renderer& renderer, RenderPass& render_pass);
ENGINE_API void extract_render_data(Renderer& renderer, Viewport& viewport, FrameData& frame, World& world, Layermask layermask);
ENGINE_API GPUSubmission build_command_buffers(Renderer& renderer, const FrameData& frame);
ENGINE_API void submit_frame(Renderer& renderer, GPUSubmission& submission);


//void build_gpu_buffers(Renderer& renderer, FrameData& frame, Viewport viewport);
/*
struct RenderPass {
	RenderExtension* extension;

	Layermask layermask;
	struct CommandBuffer& command_buffer;
	struct Pass* pass;
	struct Skybox* skybox;
	struct DirLight* dir_light;

	glm::vec3 view_pos;
	glm::mat4 projection;
	glm::mat4 view;
	struct Camera* cam = NULL;

	glm::mat4* model_m;

	uint width = 0;
	uint height = 0;
	ENGINE_API void set_shader_scene_params(ShaderConfig&);

	ENGINE_API RenderPass(struct CommandBuffer&, struct Pass*);
	ENGINE_API RenderPass(const RenderPass&, struct CommandBuffer&, struct Pass*);
};

struct ENGINE_API PreRenderParams {
	Layermask layermask;

	PreRenderParams(Layermask layermask);
};
*/