#pragma once

#include "engine/core.h"
#include "engine/handle.h"
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

REFL
struct ShaderInfo {
	sstring vfilename;
	sstring ffilename;
	u64 v_time_modified;
	u64 f_time_modified;
};

struct DefaultShaders {
	shader_handle pbr;
	shader_handle tree;
	shader_handle grass;
	shader_handle gizmo;
};

ENGINE_API extern DefaultShaders default_shaders;

//ENGINE_API uniform_handle uniform_id(shader_handle shader, string_view name);
ENGINE_API UniformInfo*   uniform_info(uniform_handle);
ENGINE_API ShaderInfo*    shader_info(shader_handle shader);
ENGINE_API ShaderModules* get_shader_config(shader_handle handle, shader_flags flags);
