#pragma once

#include "core/reflection.h"
#include "core/handle.h"
#include "core/container/sstring.h"
#include "graphics/renderer/render_feature.h"

struct World;
struct RenderCtx;
struct AssetManager;
struct ModelRenderer;
struct ShaderConfig;

struct ENGINE_API Skybox {
	sstring filename;
	bool capture_scene = true;

	cubemap_handle env_cubemap;
	cubemap_handle irradiance_cubemap;
	cubemap_handle prefilter_cubemap;
	texture_handle brdf_LUT = { INVALID_HANDLE };

	REFLECT(NO_ARG)
};


struct SkyboxSystem : RenderFeature {
	AssetManager& asset_manager;
	model_handle cube_model;

	SkyboxSystem(AssetManager&, World&);
	
	Skybox* make_default_Skybox(World&, RenderCtx*, string_view);
	void bind_ibl_params(ShaderConfig&, RenderCtx&); //todo strange function signature
	void capture_scene(World& world, RenderCtx& ctx, ID id);
	void load(Skybox*); 
	void render(World&, struct RenderCtx&) override;
};