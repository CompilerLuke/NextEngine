#include "stdafx.h"
#include "graphics/draw.h"
#include "graphics/rhi.h"
#include <glad/glad.h>
#include "core/sort.h"

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
	true,
	draw_opaque
};

DrawCommandState draw_draw_over = {
	Cull_None,
	DepthFunc_Lequal,
	true,
	draw_over,
};

DrawCommand::DrawCommand(ID id, glm::mat4* model, AABB* aabb, VertexBuffer* vertex_buffer, Material* material) :
	id(id), model_m(model), aabb(aabb), buffer(vertex_buffer), material(material)
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

int can_instance(vector<DrawCommand>& commands, int i, World& world) {
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

void switch_shader(World& world, RenderParams& params, Handle<Shader> shader_id, bool instanced) {
	auto shader = RHI::shader_manager.get(shader_id);

	if (instanced) {
		auto instanced_shader = shader->instanced_version;
		shader = RHI::shader_manager.get(instanced_shader);
	}

	shader->bind();
	params.command_buffer->current_texture_index = 0;
	params.set_shader_scene_params(shader_id, world);
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
		glDisable(GL_DEPTH_TEST);
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

void set_params(CommandBuffer& command_buffer, Material& mat, World& world) {
	auto& params = mat.params;
	auto shader = mat.shader;

	auto previous_num_tex_index = command_buffer.current_texture_index;

	for (auto &param : params) {
		switch (param.type) {
		case Param_Vec3:
			shader::set_vec3(shader, param.loc, param.vec3);
			break;
		
		case Param_Vec2:
			shader::set_vec2(shader, param.loc, param.vec2);
			break;

		case Param_Mat4x4:
			shader::set_mat4(shader, param.loc, param.matrix); 
			break;

		case Param_Image: {
			auto index = command_buffer.next_texture_index();
			texture::bind_to(param.image, index);
			shader::set_int(shader, param.loc, index);
			break;
		}
		case Param_Cubemap: {
			auto index = command_buffer.next_texture_index();
			cubemap::bind_to(param.cubemap, index);
			shader::set_int(shader, param.loc, index);
			break;
		}
		case Param_Int:
			shader::set_int(shader, param.loc, param.integer);
			break;
		}
	}

	command_buffer.current_texture_index = previous_num_tex_index;
}

bool operator!=(Material& mat1, Material& mat2) {
	return std::memcmp(&mat1, &mat2, sizeof(Material)) != 0;
}

#include <iostream>

void CommandBuffer::submit_to_gpu(World& world, RenderParams& render_params) {
	//todo culling
	for (auto &cmd : commands) {
		cmd.key = ((long long)cmd.material->state->order << 36) 
			    + ((long long)cmd.material->state->depth_func << 32)
			    + (cmd.material->state->cull << 30) 
			    + (cmd.buffer->vao << 15) 
			    + (cmd.material->shader.id << 7) 
			    + ((long long)cmd.material % 64);
	}

	radixsort(commands.data, commands.length, [](DrawCommand& cmd) { return cmd.key; });
	bool last_was_instanced = false;

	for (unsigned int i = 0; i < commands.length;) {
		auto& cmd = commands[i];
		auto& mat = *cmd.material;

		auto num_instanceable = can_instance(commands, i, world);
		auto instanced = num_instanceable > 2;

		if (i == 0) {
			switch_shader(world, render_params, mat.shader, instanced);
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
			set_params(*this, mat, world);
		}
		else {
			auto& last_cmd = commands[i - 1];
			auto& last_mat = *last_cmd.material;

			if ((last_mat.shader.id != mat.shader.id) || last_was_instanced) {
				switch_shader(world, render_params, mat.shader, instanced);
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

			if ((mat != last_mat) || (last_was_instanced != instanced)) {
				set_params(*this, mat, world);
			}
		}

		if (instanced) {
			auto vao = cmd.buffer->vao;
			auto instance_buffer = this->instanced_buffers[cmd.key];

			if (instance_buffer == 0) {
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

				this->instanced_buffers[cmd.key] = buff;

				instance_buffer = buff;

				auto transforms = TEMPORARY_ARRAY(glm::mat4, num_instanceable);

				for (int c = 0; c < num_instanceable; c++) {
					transforms[i] = *commands[i].model_m;
					i += 1;
				}

				glBindBuffer(GL_ARRAY_BUFFER, instance_buffer);
				glBufferData(GL_ARRAY_BUFFER, num_instanceable * sizeof(glm::mat4), transforms, GL_STREAM_DRAW);
			}
		}
		else {
			shader::set_mat4(mat.shader, "model", *cmd.model_m); //todo cache model uniform lookup
			if (cmd.material->state->mode == DrawSolid) {
				glDrawElements(GL_TRIANGLES, cmd.buffer->length, GL_UNSIGNED_INT, NULL);
			}
			else {
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				glDrawElements(GL_TRIANGLES, cmd.buffer->length, GL_UNSIGNED_INT, NULL);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			i += 1;
		}
		last_was_instanced = instanced;
	}
}