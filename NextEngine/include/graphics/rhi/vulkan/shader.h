#pragma once

#include "core/core.h"
#include "graphics/assets/shader.h"
#include "vulkan.h"

typedef struct shaderc_compiler* shaderc_compiler_t;
using ShaderModule = VkShaderModule;
using shader_flags = u64;

enum Stage {
	VERTEX_STAGE,
	FRAGMENT_STAGE
};

struct ShaderModules {
	VkShaderModule vert;
	VkShaderModule frag;
};

struct Shader {
	ShaderInfo info;
	array<10, shader_flags> config_flags;
	array<10, ShaderModules> configs;
};

VkShaderModule make_ShaderModule(VkDevice device, string_view code);
string_buffer compile_glsl_to_spirv(Assets& assets, shaderc_compiler_t compiler, Stage stage, string_view source, string_view input_file_name, shader_flags flags, string_buffer* err);