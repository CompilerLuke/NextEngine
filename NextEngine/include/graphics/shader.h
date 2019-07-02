#pragma once

#include <string>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "ecs/ecs.h"
#include "reflection/reflection.h"
#include "core/handle.h"

struct Shader;

struct Uniform {
	Uniform(const std::string& name = "", int id = 0);

	std::string name;
	int id;

	REFLECT()
};

namespace shader {
	void set_mat4(Handle<Shader> shader_handle, Handle<Uniform> uniform, glm::mat4&);
	void set_vec3(Handle<Shader> shader_handle, Handle<Uniform> uniform, glm::vec3&);
	void set_vec2(Handle<Shader> shader_handle, Handle<Uniform> uniform, glm::vec2&);
	void set_int(Handle<Shader> shader_handle, Handle<Uniform> uniform, int);
	void set_float(Handle<Shader> shader_handle, Handle<Uniform> uniform, float);

	void set_mat4(Handle<Shader> shader_handle, const char*, glm::mat4&);
	void set_vec3(Handle<Shader> shader_handle, const char*, glm::vec3&);
	void set_vec2(Handle<Shader> shader_handle, const char*, glm::vec2&);
	void set_int(Handle<Shader> shader_handle, const char*, int);
	void set_float(Handle<Shader> shader_handle, const char*, float);

	void bind(Handle<Shader>);
}

struct Shader {
	std::string v_filename;
	std::string f_filename;
	int id;

	vector<Uniform> uniforms;

	long long v_time_modified = 0;
	long long f_time_modified = 0;

	bool supports_instancing = false;
	bool instanced = false;
	Handle<Shader> instanced_version = { INVALID_HANDLE };

	void load_in_place();
	void bind();

	Shader(Shader&&);

	Shader() {};
	~Shader();

	REFLECT()
};

Handle<Shader> load_Shader(const std::string& vfilename, const std::string& ffilename, bool supports_instancing = false, bool instanced = false);
Handle<Uniform> location(Handle<Shader>, const std::string&);