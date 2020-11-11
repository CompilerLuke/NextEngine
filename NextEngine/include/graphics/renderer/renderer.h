#pragma once

#include "engine/handle.h"
#include "ecs/id.h"
#include "core/container/vector.h"
#include "core/container/handle_manager.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/pass/render_pass.h"
#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/lighting_system.h"
#include "graphics/renderer/terrain.h"
#include "graphics/renderer/grass.h"
#include "graphics/renderer/ibl.h"
#include "graphics/culling/scene_partition.h"
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>
#include "frame.h"

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
	TerrainRenderData terrain_data;
	GrassRenderData grass_data;
	struct CulledMeshBucket* culled_mesh_bucket[RenderPass::ScenePassCount];
};

const uint SHADOW_CASCADES = 4;

struct UpdateMaterial {
	material_handle handle;
	MaterialDesc from;
	MaterialDesc to;
};

struct SimulationUBO {
	alignas(16) float time;
};

struct ENGINE_API Renderer {
	Viewport viewport;
	RenderSettings settings;

	ScenePartition scene_partition;
	MeshBucketCache mesh_buckets;

	LightingSystem lighting_system;
	TerrainRenderResources terrain_render_resources;

	UBOBuffer scene_pass_ubo[MAX_FRAMES_IN_FLIGHT];
	UBOBuffer simulation_ubo[MAX_FRAMES_IN_FLIGHT];
	descriptor_set_handle scene_pass_descriptor[MAX_FRAMES_IN_FLIGHT];

	//texture_handle shadow_fbo[4];
	pipeline_layout_handle z_prepass_pipeline_layout;
	pipeline_layout_handle color_pipeline_layout;

	texture_handle scene_map;
	texture_handle shadow_mask_map[SHADOW_CASCADES];

	tvector<UpdateMaterial> update_materials;
};

Renderer* make_Renderer(const RenderSettings&, World&);
void destroy_Renderer(Renderer*);

ENGINE_API void build_framegraph(Renderer& renderer, slice<Dependency>);
ENGINE_API void render_overlay(Renderer& renderer, RenderPass& render_pass);
ENGINE_API void extract_render_data(Renderer& renderer, Viewport& viewport, FrameData& frame, World& world, EntityQuery layermask);
ENGINE_API GPUSubmission build_command_buffers(Renderer& renderer, const FrameData& frame);
ENGINE_API void submit_frame(Renderer& renderer, GPUSubmission& submission);

ENGINE_API uint get_frame_index();
