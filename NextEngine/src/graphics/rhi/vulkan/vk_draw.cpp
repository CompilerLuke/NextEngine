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

#ifdef RENDER_API_VULKAN

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


CommandBuffer::CommandBuffer(Assets& assets) : assets(assets) {
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
	//Model* model = assets.models.get(handle);

	//for (Mesh& mesh : model->meshes) {
	//	material_handle mat_handle = materials[mesh.material_id];
	//	Material* mat = assets.materials.get(mat_handle);
	//	draw(model_m, &mesh.buffer, mat);
	//}
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
	//Shader* shader = shader_manager.get(shader_id);
	//ShaderConfig* config = shader_manager.get_config(shader_id, config_id);
	//config->bind();

	//*model_uniform = shader->location("model");
	//*found_config = config;

	//ctx.command_buffer.current_texture_index = 0;
	//ctx.set_shader_scene_params(*config);
}

void depth_func_bind(DepthFunc func) {
	switch (func) {
	case DepthFunc_Lequal: 
		break;
	case DepthFunc_Less: 
		break;
	case DepthFunc_None:
		break;
	}
}

void cull_bind(Cull cull) {
	switch (cull) {
	case Cull_None: 
		return;
	case Cull_Back: 
		return;
	case Cull_Front:
		return;
	}
}


void stencil_func_bind(StencilFunc func) {

	switch (func) {
	case StencilFunc_None:
		return;
	case StencilFunc_Equal:
		return;
	case StencilFunc_NotEqual:
		return;
	case StencilFunc_Always:
		return;
	};
}

void stencil_op_bind(StencilOp op) {
	if (op == Stencil_Keep_Replace) /**/;
}

void color_mask_bind(ColorMask mask) {
	if (mask == Color_All) 0; /**/
	else 0; /**/
}

void set_params(CommandBuffer& command_buffer, Assets& assets, shader_config_handle config_handle, Material& mat) {	
	/*TextureManager& texture_manager = assets.textures;
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
	*/
}

bool operator!=(Material& mat1, Material& mat2) {
	return true;
	//return std::memcmp(&mat1, &mat2, sizeof(Material)) != 0;
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

/*
shader_flags shader_type = ctx.pass->type == Pass::Depth_Only * SHADER_DEPTH_ONLY;

for (unsigned int i = 0; i < self.commands.length; i++) {
auto& cmd = self.commands[i];

int is_instanced = cmd.instance_buffer != NULL;

shader_flags shader_flags = shader_type | is_instanced * SHADER_INSTANCED;
*/

void CommandBuffer::submit_to_gpu(RenderCtx& ctx) {
	
}
#endif