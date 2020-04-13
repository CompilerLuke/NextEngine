#pragma once

#include "core/core.h"
#include "core/handle.h"
#include "core/reflection.h"
#include "core/container/sstring.h"

using shader_flags = u64;

enum {
	SHADER_INSTANCED = 1 << 0,
	SHADER_DEPTH_ONLY = 1 << 1
};

struct Assets;
struct ShaderModules;

struct UniformInfo {
	sstring name;
	enum Type {} type;
};

struct ShaderInfo {
	sstring vfilename;
	sstring ffilename;
	u64 v_time_modified;
	u64 f_time_modified;

	REFLECT(ENGINE_API)
};

ENGINE_API uniform_handle uniform_id(Assets&, shader_handle shader, string_view name);
ENGINE_API UniformInfo*   uniform_info(Assets&, uniform_handle);
ENGINE_API ShaderInfo*    shader_info(Assets&, shader_handle shader);
ENGINE_API ShaderModules* get_shader_config(Assets& assets, shader_handle handle, shader_flags flags);