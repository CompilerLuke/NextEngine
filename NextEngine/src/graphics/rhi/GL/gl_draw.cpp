#include "stdafx.h"
#include "graphics/rhi/draw.h"
#include "graphics/rhi/buffer.h"
#include <glad/glad.h>
#include "graphics/pass/pass.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/material.h"
#include "core/container/sort.h"
#include "graphics/assets/assets.h"
#include "core/io/logger.h"
#include "core/time.h"

#ifdef RENDER_API_OPENGL

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


CommandBuffer::CommandBuffer(AssetManager& assets) : assets(assets) {
	commands.allocator = &temporary_allocator;
}

void CommandBuffer::draw(glm::mat4 model, VertexBuffer* vertex_buffer, Material* material) {
	DrawCommand cmd;
	cmd.model_m = model;
	cmd.buffer = vertex_buffer;
	cmd.material = material;
	cmd.num_instances = 1;

	commands.append(cmd);
}

void CommandBuffer::draw(int length, struct VertexBuffer* vertex_buffer, struct InstanceBuffer* instance_buffer, struct Material* material) {
	DrawCommand cmd;
	cmd.instance_buffer = instance_buffer;
	cmd.buffer = vertex_buffer;
	cmd.num_instances = length;
	cmd.material = material;

	commands.append(cmd);
}

void CommandBuffer::draw(glm::mat4 model_m, model_handle handle, slice<material_handle> materials) {
	Model* model = assets.models.get(handle);

	for (Mesh& mesh : model->meshes) {
		material_handle mat_handle = materials[mesh.material_id];
		Material* mat = assets.materials.get(mat_handle);
		draw(model_m, &mesh.buffer, mat);
	}
}

CommandBuffer::~CommandBuffer() {}

unsigned int CommandBuffer::next_texture_index() {
	return current_texture_index++;
}

void CommandBuffer::clear() {
	commands.clear();
	commands.shrink_to_fit();
	//otherwise we would have memory from last frame, which may get overwritten
	commands.reserve(50);
}



void switch_shader(ShaderManager& shader_manager, RenderCtx& ctx, shader_handle shader_id, shader_config_handle config_id, ShaderConfig** found_config, uniform_handle* model_uniform) {
	Shader* shader = shader_manager.get(shader_id);
	ShaderConfig* config = shader_manager.get_config(shader_id, config_id);
	config->bind();

	*model_uniform = shader->location("model");
	*found_config = config;

	ctx.command_buffer.current_texture_index = 0;
	ctx.set_shader_scene_params(*config);
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

void color_mask_bind(ColorMask mask) {
	if (mask == Color_All) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	else glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
}

void set_params(CommandBuffer& command_buffer, AssetManager& assets, shader_config_handle config_handle, Material& mat) {	
	TextureManager& texture_manager = assets.textures;
	ShaderManager& shader_manager = assets.shaders;
	CubemapManager& cubemap_manager = assets.cubemaps;
	
	static texture_handle empty_texture = texture_manager.load("solid_white.png");
	
	auto& params = mat.params;

	ShaderConfig* config = shader_manager.get_config(mat.shader, config_handle);

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
			gl_bind_to(texture_manager, param.image, index);
			config->set_int(param.loc, index);
			break;
		}
		case Param_Channel3: {
			auto index = command_buffer.next_texture_index();

			if (param.channel3.image.id == INVALID_HANDLE) {
				gl_bind_to(texture_manager, empty_texture, index);
				config->set_int(param.loc, index);
			}
			else {
				gl_bind_to(texture_manager, param.channel3.image, index);
				config->set_int(param.loc, index);
			}

			config->set_vec3(param.channel3.scalar_loc, param.channel3.color);
			break;
		}
		case Param_Channel2: {
			auto index = command_buffer.next_texture_index();

			if (param.channel2.image.id == INVALID_HANDLE) {
				gl_bind_to(texture_manager, empty_texture, index);
				config->set_int(param.loc, index);
			}
			else {
				gl_bind_to(texture_manager, param.channel2.image, index);
				config->set_int(param.loc, index);
			}

			config->set_vec2(param.channel2.scalar_loc, param.channel2.value);
			break;
		}
		case Param_Channel1: {
			auto index = command_buffer.next_texture_index();

			if (param.channel1.image.id == INVALID_HANDLE) {
				gl_bind_to(texture_manager, empty_texture, index);
				config->set_int(param.loc, index);
			}
			else {
				gl_bind_to(texture_manager, param.channel1.image, index);
				config->set_int(param.loc, index);
			}

			config->set_float(param.channel1.scalar_loc, param.channel1.value);
			break;
		}


		case Param_Cubemap: {
			auto index = command_buffer.next_texture_index();
			gl_bind_cubemap(cubemap_manager, param.cubemap, index);
			config->set_int(param.loc, index);
			break;
		}
		case Param_Int:
			config->set_int(param.loc, param.integer);
			break;

		case Param_Float:
			config->set_float(param.loc, param.real);
			break;

		case Param_Time: 
			config->set_float(param.loc, glfwGetTime());
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
	glm::mat4 model_m;
	struct VertexBuffer* buffer;
	struct InstanceBuffer* instance_buffer;
	Material* material;

	int num_instances;

	shader_config_handle config;

	unsigned int flags;
};

struct SortKey {
	DrawSortKey key = 0;
	unsigned int index;
};

void extract_layout(SpecializedDrawCommand& cmd, VertexLayout* v_layout, InstanceLayout* layout) {
	*v_layout = cmd.buffer->layout;
	*layout = cmd.instance_buffer ? cmd.instance_buffer->layout : INSTANCE_LAYOUT_MAT4X4;
}


struct DrawElementsIndirectCommand {
	uint  count;
	uint  instanceCount;
	uint  firstIndex;
	uint  baseVertex;
	uint  baseInstance;
};

void CommandBuffer::submit_to_gpu(RenderCtx& ctx) {
	CommandBuffer& self = ctx.command_buffer;
	AssetManager& assets = self.assets;
	
	vector<SpecializedDrawCommand> commands_data;
	commands_data.allocator = &temporary_allocator;
	commands_data.reserve(self.commands.length);

	vector<SortKey> commands;
	commands.allocator = &temporary_allocator;
	commands.reserve(self.commands.length);

	ShaderConfigDesc::ShaderType shader_type = ctx.pass->type == Pass::Standard ? ShaderConfigDesc::Standard : ShaderConfigDesc::DepthOnly;

	for (unsigned int i = 0; i < self.commands.length; i++) {
		auto& cmd = self.commands[i];

		ShaderConfigDesc desc;
		desc.instanced = cmd.instance_buffer != NULL;
		desc.type = shader_type;
		desc.flags = cmd.material->shader_flags;

		SpecializedDrawCommand new_cmd;
		new_cmd.buffer = cmd.buffer;
		new_cmd.model_m = new_cmd.model_m;
		new_cmd.num_instances = cmd.num_instances;
		new_cmd.model_m = cmd.model_m;
		new_cmd.config = assets.shaders.get_config_handle(cmd.material->shader, desc);
		new_cmd.material = cmd.material;
		new_cmd.instance_buffer = cmd.instance_buffer;

		DrawSortKey key = (unsigned long long)cmd.material->state->order << 30
			//| ((unsigned long long)cmd.material->state->depth_func << 32)
			//| (cmd.material->state->cull << 30)
			//| (cmd.buffer->layout << 15)
			| (cmd.material->shader.id << 7)
			| ((unsigned long long)cmd.material % 64);

		commands_data.append(new_cmd);
		commands.append({ key, i });
	}

	radixsort(commands.data, commands.length, [](SortKey& key) { return key.key; }); //todo investigate why debug mode does not work
	bool last_was_instanced = false;

	VertexLayout vertex_layout;
	InstanceLayout instance_layout;

	uniform_handle model_uniform;
	ShaderConfig* shader_config = NULL;

	for (unsigned int i = 0; i < commands.length;) {
		DrawSortKey key = commands[i].key;
		
		auto& cmd = commands_data[commands[i].index];
		auto& mat = *cmd.material;

		auto num_instanced = cmd.num_instances; //can_instance(commands, i, world);
		auto instanced = cmd.instance_buffer != NULL;

		if (i == 0) {
			switch_shader(assets.shaders, ctx, mat.shader, cmd.config, &shader_config, &model_uniform);

			extract_layout(cmd, &vertex_layout, &instance_layout);
			RHI::bind_vertex_buffer(vertex_layout, instance_layout);

			depth_func_bind(mat.state->depth_func);
			cull_bind(mat.state->cull);
			if (mat.state->clear_depth_buffer) {
				glDepthMask(GL_FALSE);
				//glClear(GL_DEPTH_BUFFER_BIT);
			}
			if (mat.state->clear_stencil_buffer) {
				glClear(GL_STENCIL_BUFFER_BIT);
			}
			stencil_op_bind(mat.state->stencil_op);
			stencil_func_bind(mat.state->stencil_func);
			glStencilMask(mat.state->stencil_mask);
			set_params(self, assets, cmd.config, mat);
		}
		else {
			auto& last_cmd = commands_data[commands[i - 1].index];
			auto& last_mat = *last_cmd.material;

			if (mat != last_mat || cmd.config.id != last_cmd.config.id) {
				switch_shader(assets.shaders, ctx, mat.shader, cmd.config, &shader_config, &model_uniform);
			}

			VertexLayout new_vertex_layout;
			InstanceLayout new_instance_layout;

			extract_layout(cmd, &new_vertex_layout, &new_instance_layout);

			if (new_vertex_layout != vertex_layout || new_instance_layout != instance_layout) {
				RHI::bind_vertex_buffer(new_vertex_layout, new_instance_layout);
				
				vertex_layout = new_vertex_layout;
				instance_layout = new_instance_layout;
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

			if (mat.state->clear_depth_buffer != last_mat.state->clear_depth_buffer) {
				glDepthMask(mat.state->clear_depth_buffer ? GL_FALSE : GL_TRUE);
			}

			if (mat.state->color_mask != last_mat.state->color_mask) {
				color_mask_bind(mat.state->color_mask);
			}

			if (mat != last_mat || cmd.config.id != last_cmd.config.id) {
				set_params(self, assets, cmd.config, mat);
			}
		}

		//AUTOMATIC BATCHING

		/*VertexBuffer* vertex_buffer = cmd.buffer;
		vector<DrawElementsIndirectCommand> multi;
		multi.allocator = &temporary_allocator;

		while (i + 1 < commands.length) {
			SpecializedDrawCommand& next_cmd = commands_data[commands[i].index];
		
			if (commands[i].key == key) {
				if (multi.length == 0 || next_cmd.buffer != vertex_buffer) {
					DrawElementsIndirectCommand multi_cmd;
					multi_cmd.baseInstance = 
				}
				else {
					multi.last().instanceCount++;
				}
			}
		}*/

		if (instanced) {
			//log("DRAWING ", cmd.instance_buffer->base, " ", num_instanced, "\n");
			glDrawElementsInstancedBaseInstance(GL_TRIANGLES, cmd.buffer->length, GL_UNSIGNED_INT, (void*)(cmd.buffer->index_offset), num_instanced, cmd.instance_buffer->base);
		}
		else {
			shader_config->set_mat4(model_uniform, cmd.model_m);
			if (cmd.material->state->mode == DrawSolid) {
				glDrawElements(GL_TRIANGLES, cmd.buffer->length, GL_UNSIGNED_INT, (void*)(cmd.buffer->index_offset));
			}
			else {
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); //todo make setting in draw
				glLineWidth(5.0);
				glDrawElements(GL_TRIANGLES, cmd.buffer->length, GL_UNSIGNED_INT, (void*)(cmd.buffer->index_offset));
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
		}
		last_was_instanced = instanced;
		i += 1;
	}

	glDepthMask(GL_TRUE);
	glDisable(GL_STENCIL_TEST);
}

#endif