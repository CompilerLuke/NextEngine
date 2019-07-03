#include "stdafx.h"
#include "graphics/shader.h"
#include "core/vfs.h"
#include "logger/logger.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include "ecs/layermask.h"
#include "graphics/rhi.h"
#include "core/vector.h"

REFLECT_GENERIC_STRUCT_BEGIN(Handle<Shader>)
REFLECT_STRUCT_MEMBER(id)
REFLECT_STRUCT_END()

REFLECT_GENERIC_STRUCT_BEGIN(Handle<Uniform>)
REFLECT_STRUCT_MEMBER(id)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Shader)
REFLECT_STRUCT_MEMBER(v_filename)
REFLECT_STRUCT_MEMBER(f_filename)
REFLECT_STRUCT_MEMBER(supports_instancing)
REFLECT_STRUCT_END()

Handle<Shader> load_Shader(const std::string& vfilename, const std::string& ffilename, bool supports_instancing, bool instanced) {
	auto& existing_shaders = RHI::shader_manager.slots;

	for (int i = 0; i < existing_shaders.length; i++) { //todo move to ShaderManager
		if (existing_shaders[i].generation == INVALID_SLOT) continue;
		auto& existing_shader = existing_shaders[i].obj;

		if (existing_shader.v_filename == vfilename && existing_shader.f_filename == ffilename && (!supports_instancing || supports_instancing == existing_shader.supports_instancing)) {
				return RHI::shader_manager.index_to_handle(i);
		}
	}

	Shader shad;

	shad.v_filename = vfilename;
	shad.f_filename = ffilename;

	shad.supports_instancing = supports_instancing;
	shad.instanced = instanced;

	shad.load_in_place();

	log("loaded shader : ", shad.v_filename, " ", shad.f_filename, shad.id);

	return RHI::shader_manager.make(std::move(shad));
}

Shader::Shader(Shader&& other) {
	this->f_filename = std::move(other.f_filename);
	this->v_filename = std::move(other.v_filename);
	this->f_time_modified = other.f_time_modified;
	this->v_time_modified = other.v_time_modified;
	this->id = other.id;
	this->instanced = other.instanced;
	this->instanced_version = other.instanced_version;
	this->supports_instancing = other.supports_instancing;
	this->uniforms = std::move(other.uniforms);

	other.id = -1;
}

Shader::~Shader() {
	if (this->id == -1) return;
	glDeleteProgram(id);
}

GLint make_shader(const std::string& filename, const std::string& source, GLenum kind, char* info_log) {
	auto c_source = source.c_str();
	auto shader = glCreateShader(kind);

	glShaderSource(shader, 1, &c_source, NULL);
	glCompileShader(shader);

	int sucess = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &sucess);

	if (!sucess) {
		glGetShaderInfoLog(shader, 512, NULL, info_log);
		if (kind == GL_VERTEX_SHADER) {
			throw std::string("(") + filename + std::string(") Vertex shader compilation: ") + std::string(info_log);
		}
		else {
			throw std::string("(") + filename + std::string(") Fragment shader compilation: ") + std::string(info_log);
		}
	}

	return shader;
}

void Shader::load_in_place() {
	std::string vshader_source;
	std::string fshader_source;

	std::string prefix;
	if (this->instanced) prefix = "#version 440 core\n#define IS_INSTANCED\n#line 0\n";
	else prefix = "#version 440 core\n#line 0\n";

	vshader_source = prefix;
	fshader_source = prefix;

	{
		File vshader_f;
		vshader_f.open(v_filename);

		File fshader_f;
		fshader_f.open(f_filename);

		vshader_source += vshader_f.read();
		fshader_source += fshader_f.read();
	}

	int sucess = 0;
	char info_log[512];

	auto vertex_shader = make_shader(v_filename, vshader_source, GL_VERTEX_SHADER, info_log);
	auto fragment_shader = make_shader(f_filename, fshader_source, GL_FRAGMENT_SHADER, info_log);

	auto id = glCreateProgram();

	glAttachShader(id, vertex_shader);
	glAttachShader(id, fragment_shader);

	glLinkProgram(id);

	glGetProgramiv(id, GL_LINK_STATUS, &sucess);

	if (!sucess) {
		glGetProgramInfoLog(id, 512, NULL, info_log);
		throw std::string("(") + v_filename + std::string(") Shader linkage : ") + std::string(info_log);
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	this->id = id;

	this->v_time_modified = Level::time_modified(v_filename);
	this->f_time_modified = Level::time_modified(f_filename);

	if (this->supports_instancing && !this->instanced) {
		this->instanced_version = load_Shader(v_filename, f_filename, true, true);
	}
}

void Shader::bind() {
	glUseProgram(id);
}

Handle<Uniform> location(Handle<Shader> handle, const std::string& name) {
	Shader* shader = RHI::shader_manager.get(handle);

	for (unsigned int i = 0; i < shader->uniforms.length; i++) {
		if (shader->uniforms[i].name == name) {
			return { i };
		}
	}

	shader->uniforms.append(Uniform(name, glGetUniformLocation(shader->id, name.c_str())));

	return { shader->uniforms.length - 1 };
}

Uniform::Uniform(const std::string& name, int id) :
	name(name),
	id(id)
{
}

Uniform* get_uniform(Handle<Shader> shader_handle, Handle<Uniform> uniform_handle) {
	Shader* shader = RHI::shader_manager.get(shader_handle);
	return &shader->uniforms[uniform_handle.id];
}

namespace shader {
	void bind(Handle<Shader> shader) {
		RHI::shader_manager.get(shader)->bind();
	}

	void set_mat4(Handle<Shader> shader_handle, Handle<Uniform> uniform, glm::mat4& value) {
		glUniformMatrix4fv(get_uniform(shader_handle, uniform)->id, 1, false, glm::value_ptr(value));
	}

	void set_vec3(Handle<Shader> shader_handle, Handle<Uniform> uniform, glm::vec3& value) {
		glUniform3fv(get_uniform(shader_handle, uniform)->id, 1, glm::value_ptr(value));
	}

	void set_vec2(Handle<Shader> shader_handle, Handle<Uniform> uniform, glm::vec2& value) {
		glUniform2fv(get_uniform(shader_handle, uniform)->id, 1, glm::value_ptr(value));
	}

	void set_int(Handle<Shader> shader_handle, Handle<Uniform> uniform, int value) {
		glUniform1i(get_uniform(shader_handle, uniform)->id, value);
	}

	void set_float(Handle<Shader> shader_handle, Handle<Uniform> uniform, float value) {
		glUniform1f(get_uniform(shader_handle, uniform)->id, value);
	}

	void set_mat4(Handle<Shader> shader_handle, const char* uniform, glm::mat4& value) {
		glUniformMatrix4fv(get_uniform(shader_handle, location(shader_handle, uniform))->id, 1, false, glm::value_ptr(value));
	}

	void set_vec3(Handle<Shader> shader_handle, const char* uniform, glm::vec3& value) {
		glUniform3fv(get_uniform(shader_handle, location(shader_handle, uniform))->id, 1, glm::value_ptr(value));
	}

	void set_vec2(Handle<Shader> shader_handle, const char* uniform, glm::vec2& value) {
		glUniform2fv(get_uniform(shader_handle, location(shader_handle, uniform))->id, 1, glm::value_ptr(value));
	}

	void set_int(Handle<Shader> shader_handle, const char* uniform, int value) {
		glUniform1i(get_uniform(shader_handle, location(shader_handle, uniform))->id, value);
	}

	void set_float(Handle<Shader> shader_handle, const char* uniform, float value) {
		glUniform1f(get_uniform(shader_handle, location(shader_handle, uniform))->id, value);
	}
}
