#pragma once

#include "engine/core.h"
#include "core/container/array.h"
#include "graphics/assets/shader.h"
#include "core/container/vector.h"
#include "volk.h"
#include <mutex>

using ShaderModule = VkShaderModule;
using shader_flags = u64;

typedef struct shaderc_compiler* shaderc_compiler_t;

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

//todo, could potentially be removed, very simmilar to DescriptorSetLayoutBinding
struct DescriptorBindingInfo {
	uint binding;
	uint count;
	VkShaderStageFlags stage;
	VkDescriptorType type;
	sstring name;
};

//todo this is easier to understand and use as a union
struct DescriptorSetInfo {
	vector<DescriptorBindingInfo> bindings;
	vector<UBOInfo> ubos;
	vector<SamplerInfo> samplers;
};

struct ShaderModuleInfo {
	vector<DescriptorSetInfo> sets;
};

struct ShaderModules {
	VkShaderModule vert = nullptr;
	VkShaderModule frag = nullptr;
	ShaderModuleInfo info;
	vector<VkDescriptorSetLayout> set_layouts;
};

struct Shader {
	ShaderInfo info;
	array<10, shader_flags> config_flags;
	array<10, ShaderModules> configs;
	
	//std::mutex mutex;
	//Shader() = default;
	//Shader(Shader&&) = default;
	//Shader(const Shader&) = delete;
};

struct Assets;

VkShaderModule make_ShaderModule(string_view code);

//ShaderModules make_ShaderModules(ShaderCompiler&, string_view vert, string_view frag);
string_buffer compile_glsl_to_spirv(shaderc_compiler_t shader_compiler, Stage stage, string_view source, string_view input_file_name, shader_flags flags, string_buffer* err);
void reflect_module(ShaderModuleInfo& info, string_view vert_spirv, string_view frag_spirv);
void gen_descriptor_layouts(ShaderModules& shader_modules);