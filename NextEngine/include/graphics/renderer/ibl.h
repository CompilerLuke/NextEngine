#pragma once

#include "core/reflection.h"
#include "engine/handle.h"
#include "ecs/id.h"
#include "core/container/sstring.h"
#include "components/skybox.h"

struct World;
struct RenderPass;
struct Assets;
struct ModelRenderer;
struct ShaderConfig;
struct LightingSystem;

/*
struct ENGINE_API SkyboxSystem {	
	model_handle cube_model;

	SkyboxSystem(World&);
	
	Skybox* make_default_Skybox(World&, RenderPass*, string_view);
	void bind_ibl_params(ShaderConfig&, RenderPass&); //todo strange function signature
	void capture_scene(World& world, RenderPass& ctx, ID id);
	void load(Skybox*); 
};
*/

struct SkyboxRenderData {
	glm::vec3 position;
	material_handle material;
};


struct CubemapPassResources;
struct Cubemap;

CubemapPassResources* make_cubemap_pass_resources();
cubemap_handle convert_equirectangular_to_cubemap(CubemapPassResources& resources, texture_handle env_map);
cubemap_handle compute_irradiance(CubemapPassResources& resources, cubemap_handle env_map);
cubemap_handle compute_reflection(CubemapPassResources& resources, cubemap_handle env_map);
texture_handle compute_brdf_lut(uint resolution);

ENGINE_API void extract_lighting_from_cubemap(LightingSystem&, struct SkyLight& skylight);

void extract_skybox(SkyboxRenderData& data, World& world, EntityQuery layermask);
ENGINE_API void render_skybox(const SkyboxRenderData& data, RenderPass& ctx);
