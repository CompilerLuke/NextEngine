#pragma once

#include "core/core.h"
#include "core/handle.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "core/reflection.h"
#include "core/container/sstring.h"
#include "core/container/handle_manager.h"

struct Uniform {
	Uniform(string_view name = "");

	string_buffer name;

	REFLECT(ENGINE_API)
};

void ENGINE_API bind(shader_handle, shader_config_handle config = { 0 });

struct ShaderConfigDesc {
	enum ShaderType { Standard, DepthOnly };

	ShaderType type = Standard;
	bool instanced = false;

	bool operator==(ShaderConfigDesc&);

	unsigned int flags = 0;
};

struct ENGINE_API ShaderConfig {
	ShaderConfigDesc desc;
	vector<int> uniform_bindings;
	int id;

	void bind();
	void set_mat4(uniform_handle uniform, glm::mat4&);
	void set_vec3(uniform_handle uniform, glm::vec3&);
	void set_vec2(uniform_handle uniform, glm::vec2&);
	void set_int(uniform_handle uniform, int);
	void set_float(uniform_handle uniform, float);
	
	void set_mat4(const char*, glm::mat4&);
	void set_vec3(const char*, glm::vec3&);
	void set_vec2(const char*, glm::vec2&);
	void set_int(const char*, int);
	void set_float(const char*, float);

	int get_binding(uniform_handle);

	ShaderConfig();
	ShaderConfig(const ShaderConfig&);
	ShaderConfig(ShaderConfig&&);
	~ShaderConfig();
};

 struct ENGINE_API Shader {
	sstring v_filename;
	sstring f_filename;

	vector<Uniform> uniforms;
	vector<ShaderConfig> configurations;
	vector<sstring> flags;

	long long v_time_modified = 0;
	long long f_time_modified = 0;

	void bind();
	void set_mat4(const char*, glm::mat4&);
	void set_vec3(const char*, glm::vec3&);
	void set_vec2(const char*, glm::vec2&);
	void set_int(const char*, int);
	void set_float(const char*, float);
	
	uint get_flag(string_view);
	
	uniform_handle location(string_view);

	Shader() = default;
	Shader(Shader&&) = default;
	Shader(const Shader&) = delete;

	REFLECT(NO_ARG);
};

struct Level;

struct ENGINE_API ShaderManager : HandleManager<Shader, shader_handle> {
	Level& level;

	ShaderManager(Level&);
	shader_handle load(string_view vfilename, string_view ffilename, bool serialized = true);
	Shader* load_get(string_view vfilename, string_view ffilename);
	ShaderConfig* get_config(shader_handle shader, shader_config_handle config);
	shader_config_handle get_config_handle(shader_handle shader, ShaderConfigDesc&);
	uniform_handle location(shader_handle shader, string_view name);
	void reload_modified();
	void reload(shader_handle);
};
