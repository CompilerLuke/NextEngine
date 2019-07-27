#include "stdafx.h"
#include "ecs/system.h"
#include "graphics/shader.h"
#include "graphics/draw.h"
#include "graphics/renderPass.h"
#include "graphics/rhi.h"

void RenderParams::set_shader_scene_params(Handle<Shader> shader, Handle<ShaderConfig> config, World& world) {
	
	shader::set_mat4(shader, "projection", projection, config);

	shader::set_mat4(shader, "projection", projection, config);
	shader::set_mat4(shader, "view", view, config);

	shader::set_float(shader, "window_width", width, config);
	shader::set_float(shader, "window_height", height, config);

	this->pass->set_shader_params(shader, config, world, *this);
}

RenderParams::RenderParams(CommandBuffer* command_buffer, Pass* pass) : 
	command_buffer(command_buffer),
	cam(NULL),
	pass(pass) {

}