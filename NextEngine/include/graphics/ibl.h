#pragma once

#include "ecs/id.h"
#include "ecs/system.h"
#include <string>
#include "reflection/reflection.h"
#include "core/handle.h"
#include "core/string_buffer.h"

struct Skybox {
	StringBuffer filename;

	Handle<struct Cubemap> env_cubemap;
	Handle<struct Cubemap> irradiance_cubemap;
	Handle<struct Cubemap> prefilter_cubemap;
	Handle<struct Texture> brdf_LUT;

	void on_load(struct World&);
	void set_ibl_params(Handle<struct Shader>, struct World&, RenderParams&);

	REFLECT()
};

Skybox* load_Skybox(struct World&, StringView);