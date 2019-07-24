#include "stdafx.h"
#include "graphics/volumetric.h"
#include "graphics/texture.h"
#include "graphics/window.h"
#include "graphics/primitives.h"
#include "ecs/system.h"
#include "graphics/shader.h"
#include "components/lights.h"
#include "components/transform.h"
#include "components/camera.h"

FogMap::FogMap(unsigned int width, unsigned int height) {
	Handle<Texture> tex;
	
	AttachmentSettings color_attachment(tex);
	color_attachment.min_filter = Nearest;
	color_attachment.mag_filter = Nearest;
	color_attachment.wrap_s = ClampToBorder;
	color_attachment.wrap_t = ClampToBorder;

	FramebufferSettings settings;
	settings.width = width;
	settings.height = height;
	settings.depth_buffer = DepthComponent24;
	settings.color_attachments.append(color_attachment);

	this->fbo = Framebuffer(settings);
	this->map = tex;
}

VolumetricPass::VolumetricPass( Window& window, Handle<Texture> depth_prepass)
	: calc_fog(window.width / 2.0f, window.height / 2.0f),
	  depth_prepass(depth_prepass),
	  volume_shader(load_Shader("shaders/screenspace.vert", "shaders/volumetric.frag")),
	  upsample_shader(load_Shader("shaders/screenspace.vert", "shaders/volumetric_upsample.frag"))
{
}

void VolumetricPass::clear() {
	calc_fog.fbo.bind();
	calc_fog.fbo.clear_color(glm::vec4(0, 0, 0, 1));
	calc_fog.fbo.unbind();
}

void VolumetricPass::render_with_cascade(World& world, RenderParams& render_params, ShadowParams& params) {
	if (!render_params.cam) return;
	
	shader::bind(volume_shader);

	calc_fog.fbo.bind();
	
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	glBlendFunc(GL_ONE, GL_ONE);

	auto dir_light = get_dir_light(world, game_layer);
	auto dir_light_trans = world.by_id<Transform>(world.id_of(dir_light));
	if (!dir_light_trans) return;

	texture::bind_to(depth_prepass, 0);
	shader::set_int(volume_shader, "depthPrepass", 0);

	texture::bind_to(params.depth_map, 1);
	shader::set_int(volume_shader, "depthMap", 1);

	auto cam_trans = world.by_id<Transform>(world.id_of(render_params.cam));
	if (!cam_trans) return;

	shader::set_vec3(volume_shader, "camPosition", cam_trans->position);
	shader::set_vec3(volume_shader, "sunColor", dir_light->color);
	shader::set_vec3(volume_shader, "sunDirection", dir_light->direction);
	shader::set_vec3(volume_shader, "sunPosition", dir_light_trans->position);

	shader::set_float(volume_shader, "gCascadeEndClipSpace[0]", params.in_range.x);
	shader::set_float(volume_shader, "gCascadeEndClipSpace[1]", params.in_range.y);

	shader::set_mat4(volume_shader, "toLight", params.to_light);
	shader::set_mat4(volume_shader, "toWorld", params.to_world);


	shader::set_int(volume_shader, "cascadeLevel", params.cascade);
	shader::set_float(volume_shader, "endCascade", render_params.cam->far_plane);

	glm::mat4 ident(1.0);
	shader::set_mat4(volume_shader, "model", ident);

	render_quad();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	calc_fog.fbo.unbind();
}

void VolumetricPass::render_upsampled(World& world, Handle<Texture> current_frame_id, glm::mat4& proj_matrix) {
	auto volumetric_map = calc_fog.map;
	auto current_frame = current_frame_id;
	
	shader::bind(upsample_shader);

	glDisable(GL_DEPTH_TEST);

	shader::set_int(upsample_shader, "depthPrepass", 0);
	texture::bind_to(depth_prepass, 0);

	shader::set_int(upsample_shader, "volumetricMap", 1);
	texture::bind_to(volumetric_map, 1);

	shader::set_int(upsample_shader, "frameMap", 2);
	texture::bind_to(current_frame, 2);

	glm::mat4 ident(1.0);
	shader::set_mat4(upsample_shader, "model", ident);

	glm::mat4 depth_proj = glm::inverse(proj_matrix);
	shader::set_mat4(upsample_shader, "depthProj", depth_proj);

	render_quad();

	glEnable(GL_DEPTH_TEST);
}
