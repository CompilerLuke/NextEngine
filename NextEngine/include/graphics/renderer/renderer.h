#pragma once

#include "engine/handle.h"
#include "ecs/id.h"
#include "core/container/vector.h"
#include "core/container/handle_manager.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/lighting_system.h"
#include "graphics/renderer/terrain.h"
#include "graphics/renderer/grass.h"
#include "graphics/renderer/ibl.h"
#include "graphics/pass/volumetric.h"
#include "graphics/pass/shadow.h"
#include "graphics/pass/composite.h"
#include "graphics/culling/scene_partition.h"
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>
#include "frame.h"
#include <memory>

struct Dependency;
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
	uint msaa = 4;
	ShadowSettings shadow;
	VolumetricSettings volumetric;
    bool zprepass = false;
	bool hotreload_shaders = false;
};

struct Renderer;

struct FrameData {
	PassUBO pass_ubo;
	LightUBO light_ubo;
	VolumetricUBO volumetric_ubo;
	CompositeUBO composite_ubo;
	ShadowCascadeProj shadow_proj_info[MAX_SHADOW_CASCADES];

	SkyboxRenderData skybox_data;
	TerrainRenderData terrain_data;
	GrassRenderData grass_data;
	struct slice<CulledMeshBucket> culled_mesh_bucket[RenderPass::ScenePassCount];
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
	tvector<UpdateMaterial> update_materials;

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
    texture_handle depth_scene_map;
    ShadowResources shadow_resources;
    VolumetricResources volumetric_resources;
    CompositeResources composite_resources;

    FrameData frames[MAX_FRAMES_IN_FLIGHT];

    vector<std::unique_ptr<RenderSystem>> systems;

    Renderer(const RenderSettings&, World&);
    ~Renderer();

    void add(std::unique_ptr<RenderSystem>&&);

    RenderPass::ID get_output_pass_id();
    RenderPass::ID get_depth_pass_id();
    texture_handle get_output_map();
    texture_handle get_depth_map();

    RenderPrepareCtx extract_render_data(World &world, Viewport &viewport, EntityQuery layermask, EntityQuery camera_layermask);
    RenderSubmitCtx build_command_buffers(RenderPrepareCtx&);
    void end_passes(RenderSubmitCtx&);
    void submit_frame(RenderSubmitCtx&);
};

ENGINE_API void build_framegraph(Renderer& renderer, slice<Dependency>);

ENGINE_API uint get_frame_index();
