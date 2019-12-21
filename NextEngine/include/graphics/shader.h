#pragma once

#include <string>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "ecs/ecs.h"
#include "reflection/reflection.h"
#include "core/handle.h"
#include "core/core.h"
#include "core/string_buffer.h"

struct Shader;

struct Uniform {
	Uniform(StringView name = "");

	StringBuffer name;

	REFLECT(ENGINE_API)
};

struct ShaderConfig;

namespace shader {
	void ENGINE_API set_mat4(Handle<Shader> shader_handle, const char*, glm::mat4&, Handle<ShaderConfig> config = { 0 });
	void ENGINE_API set_vec3(Handle<Shader> shader_handle, const char*, glm::vec3&, Handle<ShaderConfig> config = { 0 });
	void ENGINE_API set_vec2(Handle<Shader> shader_handle, const char*, glm::vec2&, Handle<ShaderConfig> config = { 0 });
	void ENGINE_API set_int(Handle<Shader> shader_handle, const char*, int, Handle<ShaderConfig> config = { 0 });
	void ENGINE_API set_float(Handle<Shader> shader_handle, const char*, float, Handle<ShaderConfig> config = { 0 });

	ShaderConfig* get_config(Handle<Shader> shader_handle, Handle<ShaderConfig> config);

	void ENGINE_API bind(Handle<Shader>, Handle<ShaderConfig> config = { 0 });
}

struct ShaderConfigDesc {
	enum ShaderType { Standard, DepthOnly };

	ShaderType type = Standard;
	bool instanced = false;

	bool operator==(ShaderConfigDesc&);

	unsigned int flags = 0;
};

struct Shader {
	StringBuffer v_filename;
	StringBuffer f_filename;

	vector<Uniform> uniforms;
	vector<ShaderConfig> configurations;
	vector<StringBuffer> flags;

	long long v_time_modified = 0;
	long long f_time_modified = 0;

	Handle<struct ShaderConfig> get_config(ShaderConfigDesc&);

	REFLECT(ENGINE_API);
};

struct ShaderConfig {
	ShaderConfigDesc desc;
	vector<int> uniform_bindings;
	int id;

	void ENGINE_API bind();
	void ENGINE_API set_mat4(Handle<Uniform> uniform, glm::mat4&);
	void ENGINE_API set_vec3(Handle<Uniform> uniform, glm::vec3&);
	void ENGINE_API set_vec2(Handle<Uniform> uniform, glm::vec2&);
	void ENGINE_API set_int(Handle<Uniform> uniform, int);
	void ENGINE_API set_float(Handle<Uniform> uniform, float);

	int get_binding(Handle<Uniform>);

	ShaderConfig();
	ShaderConfig(ShaderConfig&&);
	~ShaderConfig();
};

Handle<Shader> ENGINE_API load_Shader(StringView vfilename, StringView ffilename, bool serialized = true);
Handle<Uniform> ENGINE_API location(Handle<Shader>, StringView);
unsigned int ENGINE_API get_flag(Handle<Shader>, StringView);

void ENGINE_API reload_shader(Shader& shad_factory);

struct DebugShaderReloadSystem{
	static void update(World&, UpdateParams&);
};