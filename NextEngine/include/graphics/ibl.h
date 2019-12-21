#pragma once

#include "ecs/id.h"
#include "ecs/system.h"
#include <string>
#include "reflection/reflection.h"
#include "core/handle.h"
#include "core/string_buffer.h"
#include "graphics/renderFeature.h";

struct ENGINE_API Skybox {
	StringBuffer filename;
	bool capture_scene = true;

	Handle<struct Cubemap> env_cubemap = { INVALID_HANDLE };
	Handle<struct Cubemap> irradiance_cubemap = { INVALID_HANDLE };
	Handle<struct Cubemap> prefilter_cubemap = { INVALID_HANDLE };
	Handle<struct Texture> brdf_LUT = { INVALID_HANDLE };

	void on_load(struct World&, bool take_capture = false, RenderParams* params = NULL);
	void set_ibl_params(Handle<struct Shader>, Handle<struct ShaderConfig>, struct World&, struct RenderParams&);

	REFLECT(NO_ARG)
};

Skybox* make_default_Skybox(struct World&, struct RenderParams*, StringView);

struct SkyboxSystem : RenderFeature {
	Handle<struct Model> cube_model;

	SkyboxSystem(struct World&);
	void render(struct World&, struct RenderParams&) override;
};