#include "stdafx.h"
#include "graphics/shadow.h"
#include "graphics/window.h"
#include "graphics/shader.h"
#include "ecs/system.h"
#include "graphics/texture.h"
#include "graphics/renderPass.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "graphics/draw.h"
#include "components/lights.h"
#include "components/transform.h"
#include "graphics/primitives.h"
#include "components/camera.h"
#include "graphics/rhi.h"

DepthMap::DepthMap(unsigned int width, unsigned int height, World& world) {
	AttachmentSettings attachment(this->depth_map);

	FramebufferSettings settings;
	settings.width = width;
	settings.height = height;
	settings.depth_attachment = &attachment;
	settings.depth_buffer = DepthComponent24;
	
	this->depth_map_FBO = Framebuffer(settings);
	this->depth_shader = load_Shader("shaders/pbr.vert", "shaders/depth.frag", true);
}

ShadowPass::ShadowPass(Window& window, World& world, Handle<Texture> depth_prepass) :
	deffered_map_cascade(4096, 4096, world),
	ping_pong_shadow_mask(window, world),
	shadow_mask(window, world),
	volumetric(window, depth_prepass),
	depth_prepass(depth_prepass),
	screenspace_blur_shader(load_Shader("shaders/screenspace.vert", "shaders/blur.frag")),
	shadow_mask_shader(load_Shader("shaders/screenspace.vert", "shaders/shadowMask.frag"))
{
}

ShadowMask::ShadowMask(Window& window, World& world) {
	AttachmentSettings color_attachment(this->shadow_mask_map);
	color_attachment.mag_filter = Linear;
	color_attachment.min_filter = Linear;
	color_attachment.wrap_s = Repeat;
	color_attachment.wrap_t = Repeat;

	FramebufferSettings settings;
	settings.width = window.width;
	settings.height = window.height;
	settings.color_attachments.append(color_attachment);
	settings.depth_buffer = DepthComponent24;

	this->shadow_mask_map_fbo = Framebuffer(settings);
}

void ShadowMask::set_shadow_params(Handle<Shader> shader, World& world, RenderParams& params) {
	auto bind_to = params.command_buffer->next_texture_index();
	texture::bind_to(shadow_mask_map, bind_to);
	shader::set_int(shader, "shadowMaskMap", bind_to);
}

void ShadowPass::set_shadow_params(Handle<Shader> shader, World& world, RenderParams& params) {
	this->shadow_mask.set_shadow_params(shader, world, params);
}

struct OrthoProjInfo {
	float endClipSpace;
	glm::mat4 toLight;
	glm::mat4 toWorld;
	glm::mat4 lightProjection;
};

constexpr int num_cascades = 4;

void calc_ortho_proj(RenderParams& params, glm::mat4& light_m, float width, float height, OrthoProjInfo shadowOrthoProjInfo[num_cascades]) {
	auto& cam_m = params.view;
	auto cam_inv_m = glm::inverse(cam_m);
	auto& proj_m = params.projection;

	if (!params.cam) throw "Can not calculate cascades without camera";

	float cascadeEnd[] = {
		params.cam->near_plane,
		20.0f,
		90.0f,
		150.0f,
		params.cam->far_plane,
	};

	for (int i = 0; i < num_cascades; i++) {
		auto proj = glm::perspective(
			glm::radians(params.cam->fov),
			(float)params.width / (float)params.height,
			cascadeEnd[i],
			cascadeEnd[i + 1]
		);

		auto frust_to_world = glm::inverse(proj * cam_m);

		glm::vec4 farFrustumCorners[5] = {
			glm::vec4(1,1,1,1),
			glm::vec4(-1,1,1,1),
			glm::vec4(1,-1,1,1),
			glm::vec4(-1,-1,1,1),
			glm::vec4(0,0,1,1) //centroid
		};
	
		for (int j = 0; j < 5; j++) {
			auto vW = frust_to_world * farFrustumCorners[j];
			vW /= vW.w;

			farFrustumCorners[j] = light_m * vW;
		}

		float radius = 0;

		for (int j = 0; j < 4; j++) {
			glm::vec3 a(farFrustumCorners[j]);
			glm::vec3 b(farFrustumCorners[4]);
			radius = std::max(radius, glm::length(a - b));
		}

		glm::vec4 centroid = farFrustumCorners[4];

		float minX = centroid.x - radius;
		float maxX = centroid.x + radius;
		float minY = centroid.y - radius;
		float maxY = centroid.z + radius;
		float minZ = 1;
		float maxZ = 200; //todo make less hardcoded

		glm::mat4 light_projection_matrix = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

		shadowOrthoProjInfo[i].toWorld = glm::inverse(proj_m * cam_m);
		shadowOrthoProjInfo[i].toLight = light_projection_matrix * light_m;
		shadowOrthoProjInfo[i].lightProjection = light_projection_matrix;
	}
}

void DepthMap::render_maps(World& world, RenderParams& params, glm::mat4 projection_m, glm::mat4 view_m) {
	CommandBuffer command_buffer;

	for (DrawCommand cmd : params.command_buffer->commands) { //makes copy of command
		auto shad = RHI::shader_manager.get(cmd.material->shader);
		if (shad->v_filename == "shaders/skybox.vert") continue; //currently different vertex shader is not supported

		auto depth_material = TEMPORARY_ALLOC(Material);
		depth_material->shader = depth_shader;
		depth_material->state = &default_draw_state;
		cmd.material = depth_material;

		command_buffer.submit(cmd);
	}

	RenderParams new_params = params;
	new_params.command_buffer = &command_buffer;
	new_params.view = view_m;
	new_params.projection = projection_m;
	new_params.pass = this;

	this->depth_map_FBO.bind();
	glEnable(GL_DEPTH_TEST);

	this->depth_map_FBO.clear_depth(glm::vec4(0, 0, 0, 1));
	
	new_params.pass = this;
	command_buffer.submit_to_gpu(world, new_params);

	this->depth_map_FBO.unbind();
}

void ShadowPass::render(World& world, RenderParams& params) {
	auto dir_light = get_dir_light(world, params.layermask);
	if (!dir_light) return;
	auto dir_light_id = world.id_of(dir_light);

	auto dir_light_trans = world.by_id<Transform>(dir_light_id);
	if (!dir_light_trans) return;

	glm::mat4 view_matrix(1.0);
	view_matrix = glm::translate(view_matrix, -dir_light_trans->position);
	view_matrix = view_matrix * glm::mat4_cast(dir_light_trans->rotation);

	auto width = this->deffered_map_cascade.depth_map_FBO.width;
	auto height = this->deffered_map_cascade.depth_map_FBO.height;

	OrthoProjInfo info[num_cascades];
	calc_ortho_proj(params, view_matrix, width, height, info);

	shadow_mask.shadow_mask_map_fbo.bind();
	shadow_mask.shadow_mask_map_fbo.clear_color(glm::vec4(0, 0, 0, 1));

	float last_clip_space = -1.0f;

	this->volumetric.clear();

	for (int i = 0; i < num_cascades; i++) {
		auto& proj_info = info[i];
		deffered_map_cascade.render_maps(world, params, proj_info.lightProjection, view_matrix);

		shadow_mask.shadow_mask_map_fbo.bind();

		glDisable(GL_DEPTH_TEST);

		shader::bind(shadow_mask_shader);

		texture::bind_to(depth_prepass, 0);
		shader::set_int(shadow_mask_shader, "depthPrepass", 0);

		texture::bind_to(shadow_mask.shadow_mask_map, 1);
		shader::set_int(shadow_mask_shader, "depthMap", 1);

		shader::set_float(shadow_mask_shader, "gCascadeEndClipSpace[0]", last_clip_space);
		shader::set_float(shadow_mask_shader, "gCascadeEndClipSpace[1]", proj_info.endClipSpace);
	
		glm::mat4 ident_matrix(1.0);
		shader::set_mat4(shadow_mask_shader, "model", ident_matrix);

		shader::set_int(shadow_mask_shader, "cascadeLevel", i);
		
		glm::vec2 in_range(last_clip_space, proj_info.endClipSpace);

		last_clip_space = proj_info.endClipSpace;

		render_quad();

		glEnable(GL_DEPTH_TEST);

		shadow_mask.shadow_mask_map_fbo.unbind();

		//compute volumetric scattering for cascade
		ShadowParams shadow_params;
		shadow_params.in_range = in_range;
		shadow_params.to_light = proj_info.toLight;
		shadow_params.to_world = proj_info.toWorld;
		shadow_params.cascade = i;
		shadow_params.depth_map = this->depth_prepass;

		volumetric.render_with_cascade(world, params, shadow_params);
	}

	//render blur
	bool horizontal = true;
	bool first_iteration = true;

	glDisable(GL_DEPTH_TEST);

	constexpr int amount = 10;

	for (int i = 0; i < amount; i++) {
		if (!horizontal) shadow_mask.shadow_mask_map_fbo.bind();
		else ping_pong_shadow_mask.shadow_mask_map_fbo.bind();

		shader::bind(screenspace_blur_shader);
		
		if (first_iteration)
			texture::bind_to(shadow_mask.shadow_mask_map, 0);
		else
			texture::bind_to(ping_pong_shadow_mask.shadow_mask_map, 0);

		shader::set_int(screenspace_blur_shader, "image", 0);
		shader::set_int(screenspace_blur_shader, "horizontal", horizontal);

		glm::mat4 m(1.0);
		shader::set_mat4(screenspace_blur_shader, "model", m);

		render_quad();

		horizontal = !horizontal;
		first_iteration = false;
	}

	shadow_mask.shadow_mask_map_fbo.unbind();
	glEnable(GL_DEPTH_TEST);
}