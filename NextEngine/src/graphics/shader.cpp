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
REFLECT_STRUCT_END()

bool load_in_place_with_err(ShaderConfig* self, StringBuffer* err, StringView v_filename, StringView f_filename);

Handle<Shader> load_Shader(StringView vfilename, StringView ffilename) {
	auto& existing_shaders = RHI::shader_manager.slots;

	for (int i = 0; i < existing_shaders.length; i++) { //todo move to ShaderManager
		auto& existing_shader = existing_shaders[i];

		if (vfilename == existing_shader.v_filename && ffilename == existing_shader.f_filename) {
			return RHI::shader_manager.index_to_handle(i);
		}
	}

	Shader shad;

	shad.v_filename = vfilename;
	shad.f_filename = ffilename;

	shad.v_time_modified = Level::time_modified(shad.v_filename);
	shad.f_time_modified = Level::time_modified(shad.f_filename);

	ShaderConfig default_config;

	StringBuffer err;
	if (!load_in_place_with_err(&default_config, &err, vfilename, ffilename)) {
		throw err;
	}

	shad.configurations.append(std::move(default_config));
	return RHI::shader_manager.make(std::move(shad));
}

ShaderConfig::ShaderConfig() {

}

ShaderConfig::ShaderConfig(ShaderConfig&& other) {
	this->uniform_bindings = std::move(other.uniform_bindings);
	this->id = other.id;
	this->desc = other.desc;
	other.id = -1;
}

ShaderConfig::~ShaderConfig() {
	if (this->id == -1) return;
	glDeleteProgram(id);
}

struct ShaderPreprocessor {
	vector<StringView> tokens;
	StringView tok;
	int i = 0;
	StringView source;

	vector<StringBuffer>& already_included;

	ShaderPreprocessor(StringView source, vector<StringBuffer>& already_included) 
	: source(source), already_included(already_included) {
	}

	void reset_tok() {
		if (tok.length > 0) {
			tokens.append(tok);
		}

		tok.data = &source.data[i + 1];
		tok.length = 0;

		tokens.append(StringView(&source.data[i], 1));
	}

	void lex() {
		tok.data = source.data;

		for (i = 0; i < source.length; i++) {
			char c = source[i];

			if (c == ' ' || c == '\n') reset_tok();
			else tok.length++;
		}

		if (tok.length > 0) {
			tokens.append(tok);
		}
	}

	StringBuffer parse() {
		lex();

		StringBuffer output;

		for (int i = 0; i < tokens.length; i++) {
			if (tokens[i] == "#include") {
				while (tokens[++i] == " ");

				auto name = tokens[i++];
				
				if (!already_included.contains(name)) {
					File file;
					file.open(name, File::ReadFile);

					StringBuffer src = file.read();

					ShaderPreprocessor pre(src, already_included);
					output += pre.parse();
				
					already_included.append(name);
				}
			}
			output += tokens[i];
		}

		return output;
	}
};

StringBuffer preprocess_source(StringView source) {
	vector<StringBuffer> already_included;
	ShaderPreprocessor pre(source, already_included);

	return pre.parse();
}

bool make_shader(StringView filename, StringView source, GLenum kind, char* info_log, GLint* result) {
	auto pre_source = preprocess_source(source);
	auto c_source = pre_source.c_str();
	
	auto shader = glCreateShader(kind);

	glShaderSource(shader, 1, &c_source, NULL);
	glCompileShader(shader);

	int sucess = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &sucess);

	if (!sucess) {
		glGetShaderInfoLog(shader, 512, NULL, info_log);
		return false;
	}
	
	*result = shader;
	return true;
}

bool load_in_place_with_err(ShaderConfig* self, StringBuffer* err, StringView v_filename, StringView f_filename) {
	ShaderConfigDesc& desc = self->desc;
	
	StringBuffer vshader_source;
	StringBuffer fshader_source;

	StringBuffer prefix = "#version 440 core\n";
	if (desc.instanced) prefix += "#define IS_INSTANCED\n";
	if (desc.type == ShaderConfigDesc::DepthOnly) prefix += "#define IS_DEPTH_ONLY\n";

	prefix += "#line 0\n";

	vshader_source = prefix;
	fshader_source = prefix;

	{
		File vshader_f;
		if (!vshader_f.open(v_filename, File::ReadFile)) return false;

		File fshader_f;
		if (!fshader_f.open(f_filename, File::ReadFile)) return false;

		vshader_source += vshader_f.read();
		fshader_source += fshader_f.read();
	}

	int sucess = 0;
	char info_log[512];

	GLint vertex_shader, fragment_shader; 
	

	if (!make_shader(v_filename, vshader_source, GL_VERTEX_SHADER, info_log, &vertex_shader)) {
		*err = format("(", v_filename, ") Vertex Shader compilation", info_log);
		return false;
	}
	if (!make_shader(f_filename, fshader_source, GL_FRAGMENT_SHADER, info_log, &fragment_shader)) {
		*err = format("(", f_filename, ") Fragment Shader compilation", info_log);
		return false;
	}

	auto id = glCreateProgram();

	glAttachShader(id, vertex_shader);
	glAttachShader(id, fragment_shader);

	glLinkProgram(id);

	glGetProgramiv(id, GL_LINK_STATUS, &sucess);

	if (!sucess) {
		glGetProgramInfoLog(id, 512, NULL, info_log);
		*err = format("(", v_filename, ") Shader linkage : ", info_log);
		return false;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	self->id = id;

	log("loaded shader : ", v_filename, " ", f_filename, self->id);

	return true;
}


void ShaderConfig::bind() {
	glUseProgram(id);
}

bool ShaderConfigDesc::operator==(ShaderConfigDesc& other) {
	return this->type == other.type && this->instanced == other.instanced;
}

Handle<Uniform> location(Handle<Shader> handle, StringView name) {
	Shader* shader = RHI::shader_manager.get(handle);

	for (unsigned int i = 0; i < shader->uniforms.length; i++) {
		if (shader->uniforms[i].name == name) {
			return { i };
		}
	}

	shader->uniforms.append(Uniform(name)); //todo uniforms should have reflection data

	for (auto& config : shader->configurations) {
		config.uniform_bindings.append(glGetUniformLocation(config.id, name.c_str()));
	}

	return { shader->uniforms.length - 1 };
}

Handle<ShaderConfig> Shader::get_config(ShaderConfigDesc& desc) {
	for (unsigned int i = 0; i < configurations.length; i++) {
		if (configurations[i].desc == desc) return { i };
	}

	ShaderConfig config;
	config.desc = desc;
	
	StringBuffer err;
	if (!load_in_place_with_err(&config, &err, v_filename, f_filename)) {
		throw err;
	}

	config.uniform_bindings.reserve(uniforms.length);
	for (auto& uniform : uniforms) {
		config.uniform_bindings.append(glGetUniformLocation(config.id, uniform.name.c_str()));
	}

	configurations.append(std::move(config));

	return { configurations.length - 1 };
}


Uniform::Uniform(StringView name) : name(name)
{
}

Uniform* get_uniform(Handle<Shader> shader_handle, Handle<Uniform> uniform_handle) {
	Shader* shader = RHI::shader_manager.get(shader_handle);
	return &shader->uniforms[uniform_handle.id];
}

int get_uniform_binding(ShaderConfig* shader, Handle<Uniform> uniform_handle) {
	return shader->uniform_bindings[uniform_handle.id];
}

void ShaderConfig::set_mat4(Handle<Uniform> uniform, glm::mat4& value) {
	glUniformMatrix4fv(get_uniform_binding(this, uniform), 1, false, glm::value_ptr(value));
}

void ShaderConfig::set_vec3(Handle<Uniform> uniform, glm::vec3& value) {
	glUniform3fv(get_uniform_binding(this, uniform), 1, glm::value_ptr(value));
}

void ShaderConfig::set_vec2(Handle<Uniform> uniform, glm::vec2& value) {
	glUniform2fv(get_uniform_binding(this, uniform), 1, glm::value_ptr(value));
}

void ShaderConfig::set_int(Handle<Uniform> uniform, int value) {
	glUniform1i(get_uniform_binding(this, uniform), value);
}

void ShaderConfig::set_float(Handle<Uniform> uniform, float value) {
	glUniform1f(get_uniform_binding(this, uniform), value);
}

int ShaderConfig::get_binding(Handle<Uniform> handle) {
	return uniform_bindings[handle.id];
}

namespace shader {
	ShaderConfig* get_config(Handle<Shader> shader, Handle<ShaderConfig> config) {
		return &RHI::shader_manager.get(shader)->configurations[config.id];
	}

	void bind(Handle<Shader> shader, Handle<ShaderConfig> config) {
		get_config(shader, config)->bind();
	}

	void set_mat4(Handle<Shader> shader_handle, const char* uniform, glm::mat4& value, Handle<ShaderConfig> config) {
		get_config(shader_handle, config)->set_mat4(location(shader_handle, uniform), value);
	}

	void set_vec3(Handle<Shader> shader_handle, const char* uniform, glm::vec3& value, Handle<ShaderConfig> config) {
		get_config(shader_handle, config)->set_vec3(location(shader_handle, uniform), value);
	}

	void set_vec2(Handle<Shader> shader_handle, const char* uniform, glm::vec2& value, Handle<ShaderConfig> config) {
		get_config(shader_handle, config)->set_vec2(location(shader_handle, uniform), value);
	}

	void set_int(Handle<Shader> shader_handle, const char* uniform, int value, Handle<ShaderConfig> config) {
		get_config(shader_handle, config)->set_int(location(shader_handle, uniform), value);
	}

	void set_float(Handle<Shader> shader_handle, const char* uniform, float value, Handle<ShaderConfig> config) {
		get_config(shader_handle, config)->set_float(location(shader_handle, uniform), value);
	}
}

void DebugShaderReloadSystem::update(World& world, UpdateParams& params) {
	if (!params.layermask & editor_layer) return;

	for (auto& shad_factory : RHI::shader_manager.slots) {
		auto v_time_modified = Level::time_modified(shad_factory.v_filename);
		auto f_time_modified = Level::time_modified(shad_factory.f_filename);

		if (v_time_modified > shad_factory.v_time_modified or f_time_modified > shad_factory.f_time_modified) {
			log("recompiled shader: ", shad_factory.v_filename);
			
			for (auto& shad : shad_factory.configurations) {
				auto previous = shad.id;

				StringBuffer err;
				if (load_in_place_with_err(&shad, &err, shad_factory.v_filename, shad_factory.f_filename)) {
					glDeleteProgram(previous);

					for (int i = 0; i < shad.uniform_bindings.length; i++) {
						shad.uniform_bindings[i] = glGetUniformLocation(shad.id, shad_factory.uniforms[i].name.c_str());
					}
				}
				else {
					shad_factory.v_time_modified = v_time_modified;
					shad_factory.f_time_modified = f_time_modified;
					log("Error ", err);
				}
			}
		}
	}
}