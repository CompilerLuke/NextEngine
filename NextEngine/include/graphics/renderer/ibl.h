#pragma once

#include "core/reflection.h"
#include "core/handle.h"
#include "core/container/sstring.h"
#include "graphics/renderer/render_feature.h"

struct World;
struct RenderCtx;
struct Assets;
struct ModelRenderer;
struct ShaderConfig;

struct Skybox { 
	sstring filename;
	bool capture_scene = true;

	cubemap_handle env_cubemap;
	cubemap_handle irradiance_cubemap;
	cubemap_handle prefilter_cubemap;
	texture_handle brdf_LUT = { INVALID_HANDLE };

	REFLECT(ENGINE_API)
};


struct ENGINE_API SkyboxSystem : RenderFeature {
	Assets& assets;
	model_handle cube_model;

	SkyboxSystem(Assets&, World&);
	
	Skybox* make_default_Skybox(World&, RenderCtx*, string_view);
	void bind_ibl_params(ShaderConfig&, RenderCtx&); //todo strange function signature
	void capture_scene(World& world, RenderCtx& ctx, ID id);
	void load(Skybox*); 
	void render(World&, struct RenderCtx&) override;
};