#include "graphics/pass/volumetric.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/primitives.h"
#include "graphics/renderer/renderer.h"
#include "graphics/renderer/lighting_system.h"

#include "components/lights.h"
#include "components/transform.h"
#include "components/camera.h"
#include "core/io/logger.h"

FogMap::FogMap(unsigned int width, unsigned int height) {
	FramebufferDesc desc{ width, height };		
	add_color_attachment(desc, &map); //Should this really be creating a handle to the actuall resource
	
	SamplerDesc sampler_desc;
	sampler_desc.min_filter = Filter::Linear;
	sampler_desc.mag_filter = Filter::Linear;
	sampler_desc.wrap_u = Wrap::ClampToBorder;
	sampler_desc.wrap_v = Wrap::ClampToBorder;

	make_Framebuffer(RenderPass::Volumetric, desc);
}

VolumetricPass::VolumetricPass(glm::vec2 size, texture_handle depth_prepass)
	: 
	  calc_fog(size.x / 2, size.y / 2),
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

void render_with_cascade(VolumetricPass& pass, glm::vec3 cam_trans, DirLightUBO& dir_light_ubo,  RenderPass& render_params, ShadowParams& params) {

	//Shader* volume_shader = assets.shaders.get(this->volume_shader);
	
	//volume_shader->bind();

	pass.calc_fog.fbo.bind();
	
	//glDisable(GL_DEPTH_TEST);
	//glEnable(GL_BLEND);

	//glBlendFunc(GL_ONE, GL_ONE);


	//gl_bind_to(assets.textures, depth_prepass, 0);
	//volume_shader->set_int("depthPrepass", 0);

	//gl_bind_to(assets.textures, params.depth_map, 1);
	//volume_shader->set_int("depthMap", 1);

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

	pass.calc_fog.fbo.unbind();
}

#include "graphics/assets/material.h"


void render_upsampled(VolumetricPass& self, World& world, texture_handle current_frame, glm::mat4& proj_matrix) {
	glm::mat4 depth_proj = glm::inverse(proj_matrix);
	
	/*RenderPass pass = begin_render_pass(RenderPass::Screen, RenderPass::Standard, self.calc_fog.fbo);
	
	MaterialDesc upsample_material_desc{ self.upsample_shader };
	mat_image(upsample_material_desc, "depthPrepass", self.depth_prepass);
	mat_image(upsample_material_desc, "volumetricMap", self.calc_fog.map);
	mat_image(upsample_material_desc, "frameMap", current_frame);
	mat_mat4(upsample_material_desc, "depthProj", depth_proj);

	material_handle upsample_material = make_Material(upsample_material_desc);

	draw_mesh(pass.cmd_buffer, primitives.quad, upsample_material, glm::mat4(1.0));

	end_render_pass(pass);
	*/
	
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
