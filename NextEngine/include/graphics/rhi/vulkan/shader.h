#pragma once

#include "core/core.h"
#include "core/container/array.h"
#include "graphics/assets/shader.h"
#include "core/container/vector.h"
#include "volk.h"

typedef struct shaderc_compiler* shaderc_compiler_t;
typedef struct shaderc_compiler_result* shaderc_compiler_result_t;
using ShaderModule = VkShaderModule;
using shader_flags = u64;

#define MAX_SET 3
#define MAX_DESCRIPTORS 10
#define MAX_UBOs 5
#define MAX_SAMPLERS 10
#define MAX_BINDING 10
#define MAX_FIELDS 5

enum Stage {
	VERTEX_STAGE,
	FRAGMENT_STAGE
};

//todo descriptor set info is massive
//maybe it's better to allocate it in a linear buffer
//current setup wastes a lot of space

struct UBOFieldInfo {
	uint offset;
	uint size;
	sstring name;
};

struct UBOInfo {
	uint id;
	uint size;
	sstring type_name;
	
	vector<UBOFieldInfo> fields;
};

struct SamplerInfo {
	uint id;
};

struct DescriptorBindingInfo {
	uint binding;
	uint count;
	VkShaderStageFlagBits stage;
	VkDescriptorType type;
	sstring name;
};

struct DescriptorSetInfo {
	vector<DescriptorBindingInfo> bindings;
	vector<UBOInfo> ubos;
	vector<SamplerInfo> samplers;
};

struct ShaderModuleInfo {
	vector<DescriptorSetInfo> sets;
};

struct ShaderModules {
	VkShaderModule vert;
	VkShaderModule frag;
	ShaderModuleInfo info;
};

struct Shader {
	ShaderInfo info;
	array<10, shader_flags> config_flags;
	array<10, ShaderModules> configs;
};


VkShaderModule make_ShaderModule(VkDevice device, string_view code);
string_buffer compile_glsl_to_spirv(Assets& assets, shaderc_compiler_t compiler, Stage stage, string_view source, string_view input_file_name, shader_flags flags, string_buffer* err);
void reflect_module(ShaderModuleInfo& info, string_view vert_spirv, string_view frag_spirv);