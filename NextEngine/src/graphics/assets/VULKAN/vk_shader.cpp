#include "stdafx.h"

#ifdef RENDER_API_VULKAN

#include "graphics/assets/shader.h"
#include "core/io/vfs.h"
#include "core/io/logger.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include "ecs/layermask.h"

REFLECT_STRUCT_BEGIN(Shader)
REFLECT_STRUCT_MEMBER(v_filename)
REFLECT_STRUCT_MEMBER(f_filename)
REFLECT_STRUCT_END()

bool load_in_place_with_err(Level& level, ShaderConfig* self, string_buffer* err, string_view v_filename, string_view f_filename, Shader* shader, bool create_flags);

shader_handle ShaderManager::load(string_view vfilename, string_view ffilename, bool serialized) {
	auto& existing_shaders = slots;

	for (int i = 0; i < existing_shaders.length; i++) { //todo move to ShaderManager
		auto& existing_shader = existing_shaders[i];

		if (vfilename == existing_shader.v_filename && ffilename == existing_shader.f_filename) {
			return index_to_handle(i);
		}
	}

	Shader shad;

	shad.v_filename = vfilename;
	shad.f_filename = ffilename;

	shad.v_time_modified = level.time_modified(shad.v_filename);
	shad.f_time_modified = level.time_modified(shad.f_filename);

	ShaderConfig default_config;

	string_buffer err;
	if (!load_in_place_with_err(level, &default_config, &err, vfilename, ffilename, &shad, true)) {
		throw err;
	}

	shad.configurations.append(std::move(default_config));
	return assign_handle(std::move(shad), serialized);
}

Shader* ShaderManager::load_get(string_view vfilename, string_view ffilename) {
	return get(load(vfilename, ffilename));
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
	/**/
}

unsigned int get_flag(Shader* shader, string_view flag_name) {
	for (unsigned int i = 0; i < shader->flags.length; i++) {
		if (shader->flags[i] == flag_name) return (1 << i);
	}

	return 0;
}

unsigned int get_flag(ShaderManager& shader_manager, shader_handle handle, string_view flag_name) {
	return get_flag(shader_manager.get(handle), flag_name);
}

/*
if (create_flags) {
if (in_struct.length > 0) { //todo move everything out of material struct
shader->flags.append(full_name);
}
else {
shader->flags.append(name);
}
}

if (get_flag(shader, full_name) & flags) {
output += format("vec3 ", name);
}
else {
output += format("sampler2D ", name);
}

/*
if (flags & flag) {
output += sampler;

i++; //,
i++; //TexCoords
i++; //)
i++;
}
else {
output += "texture(";
output += sampler;
}

*/

struct ShaderPreprocessor {
	Level& level;
	vector<string_view> tokens;
	string_view tok;
	int i = 0;
	string_view source;

	vector<string_buffer>& already_included;
	unsigned int flags;

	bool create_flags; //if this flag is set it will upon finding channel3 add the flag to the shader

	Shader* shader;

	string_buffer in_struct;

	vector<string_buffer> channels;

	ShaderPreprocessor(Level& level, string_view source, vector<string_buffer>& already_included, unsigned int flags, Shader* shader, bool create_flags)
	: level(level), source(source), already_included(already_included), flags(flags), shader(shader), create_flags(create_flags) {
	}

	void channel(unsigned int num, string_buffer& output) {
		i++;
		string_view name = tokens[++i];

		string_buffer full_name;
		if (in_struct.length > 0) full_name = format("material.", name);
		else full_name = name;

		channels.append(full_name);

		output += "sampler2D ";
		output += name;

		if (num == 3) output += "; vec3 ";
		if (num == 2) output += "; vec2 ";
		if (num == 1) output += "; float ";

		output += name;
		output += "_scalar";
	}

	void reset_tok() {
		if (tok.length > 0) {
			tokens.append(tok);
		}

		tok.data = &source.data[i + 1];
		tok.length = 0;

		tokens.append(string_view(&source.data[i], 1));
	}

	void lex() {
		tok.data = source.data;

		for (i = 0; i < source.length; i++) {
			char c = source[i];

			if (c == ' ' || c == '\n' || c == '\t' || c == ';' || c == '}' || c == '(' || c == ')' || c == '=' || c == ',') reset_tok();
			else tok.length++;
		}

		if (tok.length > 0) {
			tokens.append(tok);
		}
	}

	string_buffer parse() {
		lex();

		string_buffer output;

		for (i = 0; i < tokens.length; i++) {
			if (tokens[i] == "#include") {
				while (tokens[++i] == " ");

				auto name = tokens[i++];
				
				if (!already_included.contains(name)) {
					File file(level);
					file.open(name, File::ReadFile);

					string_buffer src = file.read();

					ShaderPreprocessor pre(level, src, already_included, flags, shader, create_flags);
					output += pre.parse();
				
					already_included.append(name);
				}
			}
			else if (tokens[i] == "texture") {
				i++; //(
				string_view sampler = tokens[++i];

				//unsigned int flag = get_flag(shader, sampler);

				if (channels.contains(sampler)) {
					log("has flag");

					output += sampler;
					output += "_scalar";
					output += " * ";
					output += "texture(";
					output += sampler;
				}
				else {
					output += "texture(";
					output += sampler;
				}
			}
			else if (tokens[i] == "struct") {
				i++;

				in_struct = tokens[++i];

				output += "struct ";
				output += in_struct;

				in_struct += ".";
			}
			else if (tokens[i] == "channel3") {
				channel(3, output);
			}
			else if (tokens[i] == "channel2") {
				channel(2, output);
			}
			else if (tokens[i] == "channel1") {
				channel(1, output);
			}
			else if (tokens[i] == "}") {
				in_struct = string_buffer();
				output += "}";
			}
			else {
				output += tokens[i];
			}
		}

		return output;
	}
};

string_buffer preprocess_source(Level& level, string_view source, unsigned int flags, Shader* shader, bool create_flags) { //todo refactor into struct
	vector<string_buffer> already_included;
	ShaderPreprocessor pre(level, source, already_included, flags, shader, create_flags);

	return pre.parse();
}

bool make_shader(Level& level, string_view filename, string_view source, GLenum kind, char* info_log, GLint* result, unsigned int flags, Shader* shader_handle, bool create_flags) {
	auto pre_source = preprocess_source(level, source, flags, shader_handle, create_flags);

	auto c_source = pre_source.c_str();
	
	auto shader = 0;
	
	/**/

	int sucess = true;
	/**/

	if (!sucess) {
		/**/
		return false;
	}
	
	*result = shader;
	return true;
}

bool load_in_place_with_err(Level& level, ShaderConfig* self, string_buffer* err, string_view v_filename, string_view f_filename, Shader* shader, bool create_flags) {
	ShaderConfigDesc& desc = self->desc;
	
	string_buffer vshader_source;
	string_buffer fshader_source;

	File preamble(level);
	if (!preamble.open("shaders/preamble.glsl", File::ReadFile)) {
		throw "Could not open preamble!";
	}

	string_buffer prefix = "#version 440 core";

	prefix += "\n";

	if (desc.instanced) prefix += "#define IS_INSTANCED\n";
	if (desc.type == ShaderConfigDesc::DepthOnly) prefix += "#define IS_DEPTH_ONLY\n";

	prefix += "#line 0\n";

	vshader_source = prefix.view();
	fshader_source = prefix.view();

	{
		File vshader_f(level);
		if (!vshader_f.open(v_filename, File::ReadFile)) return false;

		File fshader_f(level);
		if (!fshader_f.open(f_filename, File::ReadFile)) return false;

		vshader_source += vshader_f.read();
		fshader_source += fshader_f.read();
	}

	int sucess = true;
	char info_log[512];

	GLint vertex_shader, fragment_shader; 

	if (!make_shader(level, v_filename, vshader_source, GL_VERTEX_SHADER, info_log, &vertex_shader, desc.flags, shader, create_flags)) {
		*err = format("(", v_filename, ") Vertex Shader compilation", info_log);
		throw "This is an error!";
		return false;
	}
	if (!make_shader(level, f_filename, fshader_source, GL_FRAGMENT_SHADER, info_log, &fragment_shader, desc.flags, shader, create_flags)) {
		*err = format("(", f_filename, ") Fragment Shader compilation", info_log);
		throw "This is an error!";
		return false;
	}

	auto id = 0;// glCreateProgram();


	/**/
	/**/

	if (!sucess) {
		/**/
		*err = format("(", v_filename, ") Shader linkage : ", info_log);
		return false;
	}

	/**/

	self->id = id;

	log("loaded shader : ", v_filename, " ", f_filename, self->id, "\n");

	return true;
}


void ShaderConfig::bind() {
	/**/
}

bool ShaderConfigDesc::operator==(ShaderConfigDesc& other) {
	return this->type == other.type && this->instanced == other.instanced && this->flags == other.flags;
}

uniform_handle Shader::location(string_view name) {
	for (unsigned int i = 0; i < uniforms.length; i++) {
		if (uniforms[i].name == name) {
			return { i };
		}
	}

	uniforms.append(Uniform(name)); //todo uniforms should have reflection data

	for (auto& config : configurations) {
		config.uniform_bindings.append(0); //glGetUniformLocation(config.id, name.c_str()));
	}

	return { uniforms.length - 1 };
}

uniform_handle ShaderManager::location(shader_handle handle, string_view name) {
	return get(handle)->location(name);
}

shader_config_handle ShaderManager::get_config_handle(shader_handle shader_handle, ShaderConfigDesc& desc) {
	Shader* shader = get(shader_handle);
	
	for (unsigned int i = 0; i < shader->configurations.length; i++) {
		if (shader->configurations[i].desc == desc) return { i };
	}

	ShaderConfig config;
	config.desc = desc;
	
	string_buffer err;
	if (!load_in_place_with_err(level, &config, &err, shader->v_filename, shader->f_filename, shader, false)) {
		throw err;
	}

	config.uniform_bindings.reserve(shader->uniforms.length);
	for (auto& uniform : shader->uniforms) {
		config.uniform_bindings.append(0); //glGetUniformLocation(config.id, uniform.name.c_str()));
	}

	shader->configurations.append(std::move(config));

	return { shader->configurations.length - 1 };
}


Uniform::Uniform(string_view name) : name(name)
{
}

Uniform* get_uniform(ShaderManager& manager, shader_handle shader_handle, uniform_handle uniform_handle) {
	Shader* shader = manager.get(shader_handle);
	return &shader->uniforms[uniform_handle.id];
}

int get_uniform_binding(ShaderConfig* shader, uniform_handle uniform_handle) {
	return shader->uniform_bindings[uniform_handle.id];
}

int get_uniform_binding(ShaderConfig* shader, const char* uniform) {
	return 0;
}

void ShaderConfig::set_mat4(uniform_handle uniform, glm::mat4& value) {
	/**/
}

void ShaderConfig::set_vec3(uniform_handle uniform, glm::vec3& value) {
	/**/
}

void ShaderConfig::set_vec2(uniform_handle uniform, glm::vec2& value) {
	/**/
}

void ShaderConfig::set_int(uniform_handle uniform, int value) {
	/**/
}

void ShaderConfig::set_float(uniform_handle uniform, float value) {
	/**/
}

void ShaderConfig::set_mat4(const char* uniform, glm::mat4& value) {
	/**/
}

void ShaderConfig::set_vec3(const char* uniform, glm::vec3& value) {
	/**/
}

void ShaderConfig::set_vec2(const char* uniform, glm::vec2& value) {
	/**/
}

void ShaderConfig::set_int(const char* uniform, int value) {
	/**/
}

void ShaderConfig::set_float(const char* uniform, float value) {
	/**/
}

void Shader::set_int(const char* uniform, int value) {
	return configurations[0].set_int(uniform, value);
}

void Shader::set_float(const char* uniform, float value) {
	return configurations[0].set_float(uniform, value);
}

void Shader::set_vec3(const char* uniform, glm::vec3& value) {
	return configurations[0].set_vec3(uniform, value);
}

void Shader::set_vec2(const char* uniform, glm::vec2& value) {
	return configurations[0].set_vec2(uniform, value);
}

void Shader::set_mat4(const char* uniform, glm::mat4& value) {
	return configurations[0].set_mat4(uniform, value);
}

void Shader::bind() {
	return configurations[0].bind();
}

int ShaderConfig::get_binding(uniform_handle handle) {
	return uniform_bindings[handle.id];
}

void gl_bind(ShaderManager& manager, shader_handle shader, shader_config_handle config) {
	manager.get_config(shader, config)->bind();
}

ShaderManager::ShaderManager(Level& level) : level(level) {}

ShaderConfig* ShaderManager::get_config(shader_handle shader, shader_config_handle config) {
	return &get(shader)->configurations[config.id];
}

void ShaderManager::reload(shader_handle handle) {
	Shader& shad_factory = *get(handle);

	log("recompiled shader: ", shad_factory.v_filename);

	for (auto& shad : shad_factory.configurations) {
		auto previous = shad.id;

		string_buffer err;
		if (load_in_place_with_err(level, &shad, &err, shad_factory.v_filename, shad_factory.f_filename, &shad_factory, false)) {
			/**/

			for (int i = 0; i < shad.uniform_bindings.length; i++) {
				shad.uniform_bindings[i] = 0; /**/
			}
		}
		else {
			log("Error ", err);
		}
	}
}

void ShaderManager::reload_modified() {
	for (int i = 0; i < slots.length; i++) {
		auto& shad_factory = slots[i];

		if (shad_factory.v_filename.length() == 0) continue;

		auto v_time_modified = level.time_modified(shad_factory.v_filename);
		auto f_time_modified = level.time_modified(shad_factory.f_filename);

		if (v_time_modified > shad_factory.v_time_modified or f_time_modified > shad_factory.f_time_modified) {
			reload(index_to_handle(i));
			
			shad_factory.v_time_modified = v_time_modified;
			shad_factory.f_time_modified = f_time_modified;
		}
	}
}

#endif