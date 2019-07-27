#pragma once

#include "ecs/id.h"
#include "ecs/system.h"
#include <string>
#include "reflection/reflection.h"
#include "core/handle.h"
#include "core/string_buffer.h"
#include "graphics/frameBuffer.h"

struct Skybox {
	StringBuffer filename;
	bool capture_scene = false;
	RenderParams* params = NULL;

	Handle<struct Cubemap> env_cubemap = { INVALID_HANDLE };
	Handle<struct Cubemap> irradiance_cubemap = { INVALID_HANDLE };
	Handle<struct Cubemap> prefilter_cubemap = { INVALID_HANDLE };
	Handle<struct Texture> brdf_LUT = { INVALID_HANDLE };

	void on_load(struct World&);
	void set_ibl_params(Handle<struct Shader>, Handle<struct ShaderConfig>, struct World&, struct RenderParams&);

	REFLECT()
};

Skybox* make_default_Skybox(struct World&, struct RenderParams*, StringView);