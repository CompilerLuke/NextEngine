#include "stdafx.h"
#include "graphics/draw.h"
#include "graphics/rhi.h"
#include <glad/glad.h>
#include "core/sort.h"
#include "graphics/pass.h"

REFLECT_BEGIN_ENUM(DrawOrder)
REFLECT_ENUM_VALUE(draw_opaque)
REFLECT_ENUM_VALUE(draw_skybox)
REFLECT_ENUM_VALUE(draw_transparent)
REFLECT_ENUM_VALUE(draw_over)
REFLECT_END_ENUM()

REFLECT_BEGIN_ENUM(Cull)
REFLECT_ENUM_VALUE(Cull_Front)
REFLECT_ENUM_VALUE(Cull_Back)
REFLECT_ENUM_VALUE(Cull_None)
REFLECT_END_ENUM()

REFLECT_BEGIN_ENUM(DepthFunc)
REFLECT_ENUM_VALUE(DepthFunc_Less)
REFLECT_ENUM_VALUE(DepthFunc_Lequal)
REFLECT_END_ENUM()

REFLECT_BEGIN_ENUM(StencilOp)
REFLECT_ENUM_VALUE(Stencil_Keep_Replace)
REFLECT_END_ENUM()

REFLECT_BEGIN_ENUM(StencilFunc)
REFLECT_ENUM_VALUE(StencilFunc_Equal)
REFLECT_ENUM_VALUE(StencilFunc_NotEqual)
REFLECT_END_ENUM()

REFLECT_STRUCT_BEGIN(DrawCommandState)
REFLECT_STRUCT_MEMBER(cull)
REFLECT_STRUCT_MEMBER(depth_func)
REFLECT_STRUCT_MEMBER(clear_depth_buffer)
REFLECT_STRUCT_MEMBER(clear_stencil_buffer)
REFLECT_STRUCT_MEMBER(order)
REFLECT_STRUCT_MEMBER(stencil_func)
REFLECT_STRUCT_MEMBER(stencil_op)
REFLECT_STRUCT_MEMBER(stencil_mask)
REFLECT_STRUCT_END()

#include "graphics/texture.h"

DrawCommandState default_draw_state = {
	Cull_None,
	DepthFunc_Lequal,
	false,
	draw_opaque
};

DrawCommandState draw_draw_over = {
	Cull_None,
	DepthFunc_None,
	false,
	draw_over,
};

DrawCommand::DrawCommand(ID id, glm::mat4* model, VertexBuffer* vertex_buffer, Material* material) :
	id(id), model_m(model), buffer(vertex_buffer), material(material)
{}

CommandBuffer::CommandBuffer() {
	commands.allocator = &temporary_allocator;
}

CommandBuffer::~CommandBuffer() {}

unsigned int CommandBuffer::next_texture_index() {
	return current_texture_index++;
}

void CommandBuffer::submit(DrawCommand& cmd) {
	commands.append(cmd);
}

void CommandBuffer::clear() {
	commands.clear();
	commands.shrink_to_fit();
	//otherwise we would have memory from last frame, which may get overwritten
	commands.reserve(50);
}

/*
int can_instance(vector<SpecializedDrawCommand>& commands, int i, World& world) {
	auto count = 1;

	auto shader = RHI::shader_manager.get(commands[i].material->shader);
	if (!shader) return 0;

	auto instanced_version = shader->instanced_version;
	if (instanced_version.id == INVALID_HANDLE) return 1;

	while (i + 1 < commands.length) {
		if (commands[i].key != commands[i + 1].key) break;

		count += 1;
		i += 1;
	}

	return count;
}
*/


void switch_shader(World& world, RenderParams& params, Handle<Shader> shader_id, Handle<ShaderConfig> config) {
	shader::get_config(shader_id, config)->bind();

	params.command_buffer->current_texture_index = 0;
	params.set_shader_scene_params(shader_id, config, world);
}

void depth_func_bind(DepthFunc func) {
	switch (func) {
	case DepthFunc_Lequal: 
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		break;
	case DepthFunc_Less: 
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS); 
		break;
	case DepthFunc_None:
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);
		break;
	}
}

void cull_bind(Cull cull) {


	switch (cull) {
	case Cull_None: 
		glDisable(GL_CULL_FACE); 
		return;
	case Cull_Back: 
		glEnable(GL_CULL_FACE); 
		glCullFace(GL_BACK);
		return;
	case Cull_Front:
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		return;
	}
}


void stencil_func_bind(StencilFunc func) {

	switch (func) {
	case StencilFunc_None:
		glDisable(GL_STENCIL_TEST);
		return;
	case StencilFunc_Equal:
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_EQUAL, 1, 0xFF);
		return;
	case StencilFunc_NotEqual:
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
		return;
	case StencilFunc_Always:
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 1, 0xFF);
		return;
	};
}

void stencil_op_bind(StencilOp op) {
	if (op == Stencil_Keep_Replace) glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
}

#include "logger/logger.h"

void set_params(CommandBuffer& command_buffer, Handle<ShaderConfig> config_handle, Material& mat, World& world) {	
	static Handle<Texture> empty_texture = load_Texture("solid_white.png");
	
	auto& params = mat.params;

	ShaderConfig* config = shader::get_config(mat.shader, config_handle);

	auto previous_num_tex_index = command_buffer.current_texture_index;

	for (auto &param : params) {
		switch (param.type) {
		case Param_Vec3:
			config->set_vec3(param.loc, param.vec3);
			break;
		
		case Param_Vec2:
			config->set_vec2(param.loc, param.vec2);
			break;

		case Param_Mat4x4:
			config->set_mat4(param.loc, param.matrix);
			break;

		case Param_Image: {
			auto index = command_buffer.next_texture_index();
			texture::bind_to(param.image, index);
			config->set_int(param.loc, index);
			break;
		}
		case Param_Channel3: {
			auto index = command_buffer.next_texture_index();
			
			if (param.channel3.image.id == INVALID_HANDLE) {
				texture::bind_to(empty_texture, index);
				config->set_int(param.loc, index);
			}
			else {
				texture::bind_to(param.channel3.image, index);
				config->set_int(param.loc, index);
			}

			config->set_vec3(param.channel3.scalar_loc, param.channel3.color);
			break;
		}
		case Param_Channel2: {
			auto index = command_buffer.next_texture_index();

			if (param.channel2.image.id == INVALID_HANDLE) {
				texture::bind_to(empty_texture, index);
				config->set_int(param.loc, index);
			}
			else {
				texture::bind_to(param.channel2.image, index);
				config->set_int(param.loc, index);
			}

			config->set_vec2(param.channel2.scalar_loc, param.channel2.value);
			break;
		}
		case Param_Channel1: {
			auto index = command_buffer.next_texture_index();

			if (param.channel1.image.id == INVALID_HANDLE) {
				texture::bind_to(empty_texture, index);
				config->set_int(param.loc, index);
			}
			else {
				texture::bind_to(param.channel1.image, index);
				config->set_int(param.loc, index);
			}

			config->set_float(param.channel1.scalar_loc, param.channel1.value);
			break;
		}


		case Param_Cubemap: {
			auto index = command_buffer.next_texture_index();
			cubemap::bind_to(param.cubemap, index);
			config->set_int(param.loc, index);
			break;
		}
		case Param_Int:
			config->set_int(param.loc, param.integer);
			break;

		case Param_Float:
			config->set_float(param.loc, param.real);
			break;
		}
	}

	command_buffer.current_texture_index = previous_num_tex_index;
}

bool operator!=(Material& mat1, Material& mat2) {
	return std::memcmp(&mat1, &mat2, sizeof(Material)) != 0;
}

#include <iostream>

bool DrawCommandState::operator==(DrawCommandState& other) {
	return memcmp(this, &other, sizeof(DrawCommandState));
}

/*for (Pipeline& pipeline : pipeline_cache) {
			if (pipeline.shader.id == cmd.material->shader.id && pipeline.config.id == config.id && pipeline.state == *cmd.material->state) {
				new_cmd.pipeline = &pipeline;
				break;
			}
		}

		if (new_cmd.pipeline == NULL) {
			Pipeline p;
			p.config = config;
			p.shader = cmd.material->shader;
			p.projection_u = location(p.shader, "projection");
			p.view_u = location(p.shader, "view");
			p.model_u = location(p.shader, "model");
			p.state = *cmd.material->state;

			pipeline_cache.append(std::move(p));

			new_cmd.pipeline = &pipeline_cache[pipeline_cache.length - 1];
}*/

struct SpecializedDrawCommand {
	glm::mat4* model_m;
	struct VertexBuffer* buffer;
	Material* material;
	int num_instances;

	Handle<ShaderConfig> config;
};

struct SortKey {
	DrawSortKey key = 0;
	unsigned int index;
};

struct InstancedBuffer {
	unsigned int buffer = 0;
	int length = 0;

	static std::unordered_map<DrawSortKey, InstancedBuffer> buffers;
};

std::unordered_map<DrawSortKey, InstancedBuffer> InstancedBuffer::buffers;

void CommandBuffer::submit_to_gpu(World& world, RenderParams& render_params) {
	vector<SpecializedDrawCommand> commands_data;
	commands_data.allocator = &temporary_allocator;
	commands_data.reserve(this->commands.length);

	vector<SortKey> commands;
	commands.allocator = &temporary_allocator;
	commands.reserve(this->commands.length);

	ShaderConfigDesc::ShaderType shader_type = render_params.pass->type == Pass::Standard ? ShaderConfigDesc::Standard : ShaderConfigDesc::DepthOnly;

	for (unsigned int i = 0; i < this->commands.length; i++) {
		auto& cmd = this->commands[i];

		bool is_instanced = cmd.num_instances > 1;

		Shader* shader = RHI::shader_manager.get(cmd.material->shader);

		ShaderConfigDesc desc;
		desc.instanced = is_instanced;
		desc.type = shader_type;
		desc.flags = cmd.material->shader_flags;

		SpecializedDrawCommand new_cmd;
		new_cmd.buffer = cmd.buffer;
		new_cmd.model_m = new_cmd.model_m;
		new_cmd.num_instances = cmd.num_instances;
		new_cmd.model_m = cmd.model_m;
		new_cmd.config = shader->get_config(desc);
		new_cmd.material = cmd.material;

		DrawSortKey key = ((long long)cmd.material->state->order << 36)
			+ ((long long)cmd.material->state->depth_func << 32)
			+ (cmd.material->state->cull << 30)
			+ (cmd.buffer->vao << 15)
			+ (cmd.material->shader.id << 7)
			+ ((long long)cmd.material % 64);

		commands_data.append(new_cmd);
		commands.append({ key, i });
	}

	radixsort(commands.data, commands.length, [](SortKey& key) { return key.key; });
	bool last_was_instanced = false;

	for (unsigned int i = 0; i < commands.length;) {
		DrawSortKey key = commands[i].key;
		
		auto& cmd = commands_data[commands[i].index];
		auto& mat = *cmd.material;

		auto num_instanceable = cmd.num_instances; //can_instance(commands, i, world);
		auto instanced = num_instanceable > 1;

		if (i == 0) {
			switch_shader(world, render_params, mat.shader, cmd.config);
			cmd.buffer->bind();
			depth_func_bind(mat.state->depth_func);
			cull_bind(mat.state->cull);
			if (mat.state->clear_depth_buffer) {
				glClear(GL_DEPTH_BUFFER_BIT);
			}
			if (mat.state->clear_stencil_buffer) {
				glClear(GL_STENCIL_BUFFER_BIT);
			}
			stencil_op_bind(mat.state->stencil_op);
			stencil_func_bind(mat.state->stencil_func);
			glStencilMask(mat.state->stencil_mask);
			set_params(*this, cmd.config, mat, world);
		}
		else {
			auto& last_cmd = commands_data[commands[i - 1].index];
			auto& last_mat = *last_cmd.material;

			if (mat != last_mat || cmd.config.id != last_cmd.config.id) {
				switch_shader(world, render_params, mat.shader, cmd.config);
			}

			if (last_cmd.buffer->vao != cmd.buffer->vao) {
				cmd.buffer->bind();
			}

			if (last_mat.state->cull != mat.state->cull) {
				cull_bind(mat.state->cull);
			}

			if (last_mat.state->depth_func != mat.state->depth_func) {
				depth_func_bind(mat.state->depth_func);
			}

			if (last_mat.state->stencil_op != mat.state->stencil_op) {
				stencil_op_bind(mat.state->stencil_op);
			}

			if (last_mat.state->stencil_func != mat.state->stencil_func) {
				stencil_func_bind(mat.state->stencil_func);
			}

			if (last_mat.state->stencil_mask != mat.state->stencil_mask) {
				glStencilMask(mat.state->stencil_mask);
			}

			if (mat.state->clear_stencil_buffer && !last_mat.state->clear_stencil_buffer) {
				glClear(GL_STENCIL_BUFFER_BIT);
			}

			if (mat.state->clear_depth_buffer && !last_mat.state->clear_depth_buffer) {
				glClear(GL_DEPTH_BUFFER_BIT);
			}

			if (mat != last_mat || cmd.config.id != last_cmd.config.id) {
				set_params(*this, cmd.config, mat, world);
			}
		}

		if (instanced) {
			auto vao = cmd.buffer->vao;
			auto instance_buffer = InstancedBuffer::buffers[key];

			if (instance_buffer.buffer == 0) {
				unsigned int buff;
				glGenBuffers(1, &buff);

				cmd.buffer->bind();

				glBindBuffer(GL_ARRAY_BUFFER, buff);
				glBufferData(GL_ARRAY_BUFFER, num_instanceable * sizeof(glm::mat4), (void*)0, GL_STREAM_DRAW);

				for (int attrib = 0; attrib < 4; attrib++) {
					glEnableVertexAttribArray(5 + attrib); //todo make this less hardcoded
					glVertexAttribPointer(5 + attrib, 4, GL_FLOAT, false, sizeof(glm::mat4), (void*)(attrib * 4 * sizeof(float)));
				}
			
				for (int attrib = 0; attrib < 4; attrib++) {
					glVertexAttribDivisor(5 + attrib, 1);
				}

				InstancedBuffer::buffers[key] = { buff, num_instanceable };

				instance_buffer = { buff, num_instanceable };

				glBufferSubData(GL_ARRAY_BUFFER, 0, num_instanceable * sizeof(glm::mat4), cmd.model_m);
			}

			glBindBuffer(GL_ARRAY_BUFFER, instance_buffer.buffer);
			//glBufferSubData(GL_ARRAY_BUFFER, 0, num_instanceable * sizeof(glm::mat4), cmd.model_m); //todo allow optimizations
			
			if (instance_buffer.length == num_instanceable) {
				//glBufferSubData(GL_ARRAY_BUFFER, 0, num_instanceable * sizeof(glm::mat4), cmd.model_m); //todo allow optimizations
			}
			else {
				glBufferData(GL_ARRAY_BUFFER, num_instanceable * sizeof(glm::mat4), cmd.model_m, GL_STREAM_DRAW);
				InstancedBuffer::buffers[key].length = num_instanceable;

				log("submitted ", num_instanceable);
			}

			glDrawElementsInstanced(GL_TRIANGLES, cmd.buffer->length, GL_UNSIGNED_INT, NULL, num_instanceable);
		}
		else {
			shader::get_config(mat.shader, cmd.config)->set_mat4(location(mat.shader, "model"), *cmd.model_m); //todo cache model uniform lookup
			if (cmd.material->state->mode == DrawSolid) {
				glDrawElements(GL_TRIANGLES, cmd.buffer->length, GL_UNSIGNED_INT, NULL);
			}
			else {
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				glDrawElements(GL_TRIANGLES, cmd.buffer->length, GL_UNSIGNED_INT, NULL);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
		}
		last_was_instanced = instanced;
		i += 1;
	}
}