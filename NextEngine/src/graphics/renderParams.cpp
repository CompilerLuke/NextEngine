#include "stdafx.h"
#include "ecs/system.h"
#include "graphics/shader.h"
#include "graphics/draw.h"
#include "graphics/renderPass.h"
#include "graphics/rhi.h"

void RenderParams::set_shader_scene_params(Handle<Shader> shader, World& world) {
	this->pass->set_shader_params(shader, world, *this);
}

RenderParams::RenderParams(CommandBuffer* command_buffer, Pass* pass) : 
	command_buffer(command_buffer),
	cam(NULL),
	pass(pass) {

}