#include "stdafx.h"
#include "graphics/pass/volumetric.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/primitives.h"
#include "graphics/renderer/renderer.h"

#include "components/lights.h"
#include "components/transform.h"
#include "components/camera.h"
#include "core/io/logger.h"

FogMap::FogMap(Assets& assets, unsigned int width, unsigned int height) {
	texture_handle tex;
	
	AttachmentDesc color_attachment(tex); //Should this really be creating a handle to the actuall resource
	color_attachment.min_filter = Filter::Linear;
	color_attachment.mag_filter = Filter::Linear;
	color_attachment.wrap_s = Wrap::ClampToBorder;
	color_attachment.wrap_t = Wrap::ClampToBorder;

	FramebufferDesc settings;
	settings.width = width;
	settings.height = height;
	settings.depth_buffer = DepthComponent24;
	settings.color_attachments.append(color_attachment);

	this->fbo = Framebuffer(assets, settings);
	this->map = tex;
}

VolumetricPass::VolumetricPass(Assets& assets, glm::vec2 size, texture_handle depth_prepass)
	: assets(assets),
	  calc_fog(assets, size.x / 2, size.y / 2),
	  depth_prepass(depth_prepass),
	  volume_shader(load_Shader(assets, "shaders/screenspace.vert", "shaders/volumetric.frag")),
	  upsample_shader(load_Shader(assets, "shaders/screenspace.vert", "shaders/volumetric_upsample.frag"))
{
}

void VolumetricPass::clear() {
	calc_fog.fbo.bind();
	calc_fog.fbo.clear_color(glm::vec4(0, 0, 0, 1));
	calc_fog.fbo.unbind();
}

struct VolumeUBO {
	glm::vec4 cam_position;
	glm::vec4 sun_color;
	glm::vec4 sun_direction;
	glm::vec4 sun_position;
	glm::mat4 to_light;
	glm::mat4 to_world;
	int cascade_level;
	int cascade_end;
};

void VolumetricPass::render_with_cascade(World& world, RenderCtx& render_params, ShadowParams& params) {
	if (!render_params.cam) return;

	//Shader* volume_shader = assets.shaders.get(this->volume_shader);
	
	//volume_shader->bind();

	calc_fog.fbo.bind();
	
	//glDisable(GL_DEPTH_TEST);
	//glEnable(GL_BLEND);

	//glBlendFunc(GL_ONE, GL_ONE);

	auto dir_light = get_dir_light(world, GAME_LAYER);
	auto dir_light_trans = world.by_id<Transform>(world.id_of(dir_light));
	if (!dir_light_trans) return;

	//gl_bind_to(assets.textures, depth_prepass, 0);
	//volume_shader->set_int("depthPrepass", 0);

	//gl_bind_to(assets.textures, params.depth_map, 1);
	//volume_shader->set_int("depthMap", 1);

	auto cam_trans = world.by_id<Transform>(world.id_of(render_params.cam));
	if (!cam_trans) return;

	//volume_shader->set_vec3("camPosition", cam_trans->position);
	//volume_shader->set_vec3("sunColor", dir_light->color);
	//volume_shader->set_vec3("sunDirection", dir_light->direction);
	//volume_shader->set_vec3("sunPosition", dir_light_trans->position);

	//volume_shader->set_float("gCascadeEndClipSpace[0]", params.in_range.x);
	//volume_shader->set_float("gCascadeEndClipSpace[1]", params.in_range.y);

	//volume_shader->set_mat4("toLight", params.to_light);
	//volume_shader->set_mat4("toWorld", params.to_world);

	//volume_shader->set_int("cascadeLevel", params.cascade);
	//volume_shader->set_int("endCascade", render_params.cam->far_plane);

	//glm::mat4 ident(1.0);
	//volume_shader->set_mat4("model", ident);

	//render_quad();

	//glDisable(GL_BLEND);
	//glEnable(GL_DEPTH_TEST);

	calc_fog.fbo.unbind();
}

void VolumetricPass::render_upsampled(World& world, texture_handle current_frame_id, glm::mat4& proj_matrix) {
	auto volumetric_map = calc_fog.map;
	auto current_frame = current_frame_id;
	
	//Shader* upsample_shader = assets.shaders.get(this->upsample_shader);

	//glDisable(GL_DEPTH_TEST);

	/*upsample_shader->bind();

	upsample_shader->set_int("depthPrepass", 0);
	gl_bind_to(assets.textures, depth_prepass, 0);

	upsample_shader->set_int("volumetricMap", 1);
	gl_bind_to(assets.textures, volumetric_map, 1);

	upsample_shader->set_int("frameMap", 2);
	gl_bind_to(assets.textures, current_frame, 2);

	glm::mat4 ident(1.0);
	upsample_shader->set_mat4("model", ident);

	glm::mat4 depth_proj = glm::inverse(proj_matrix);

	upsample_shader->set_mat4("depthProj", depth_proj);*/

	//render_quad();

	//glEnable(GL_DEPTH_TEST);
}
