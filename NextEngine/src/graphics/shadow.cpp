#include "stdafx.h"
#include "graphics/shadow.h"
#include "graphics/window.h"
#include "graphics/shader.h"
#include "ecs/system.h"
#include "graphics/texture.h"
#include "graphics/renderPass.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "graphics/draw.h"
#include "components/lights.h"
#include "components/transform.h"
#include "graphics/primitives.h"
#include "components/camera.h"
#include "graphics/rhi.h"
#include "logger/logger.h"
#include "graphics/renderer.h"

DepthMap::DepthMap(unsigned int width, unsigned int height, World& world, bool stencil) {
	this->type = Pass::Depth_Only;
	
	AttachmentSettings attachment(this->depth_map);

	FramebufferSettings settings;
	settings.width = width;
	settings.height = height;
	settings.depth_attachment = &attachment;
	settings.depth_buffer = DepthComponent24;

	//if (stencil) settings.stencil_buffer = StencilComponent8;
	
	this->depth_map_FBO = Framebuffer(settings);
	this->depth_shader = load_Shader("shaders/gizmo.vert", "shaders/depth.frag");
}

ShadowPass::ShadowPass(glm::vec2 size, World& world, Handle<Texture> depth_prepass) :
	deffered_map_cascade(4096, 4096, world),
	//deffered_map_cascade(2048, 2048, world),
	ping_pong_shadow_mask(size, world),
	shadow_mask(size, world),
	depth_prepass(depth_prepass),
	volumetric(size, depth_prepass),
	screenspace_blur_shader(load_Shader("shaders/screenspace.vert", "shaders/blur.frag")),
	shadow_mask_shader(load_Shader("shaders/screenspace.vert", "shaders/shadowMask.frag"))
{
}

ShadowMask::ShadowMask(glm::vec2 size, World& world) {
	AttachmentSettings color_attachment(this->shadow_mask_map);
	color_attachment.mag_filter = Linear;
	color_attachment.min_filter = Linear;
	color_attachment.wrap_s = Repeat;
	color_attachment.wrap_t = Repeat;

	FramebufferSettings settings;
	settings.width = size.x;
	settings.height = size.y;
	settings.color_attachments.append(color_attachment);
	settings.depth_buffer = DepthComponent24;


	this->shadow_mask_map_fbo = Framebuffer(settings);
}

void ShadowMask::set_shadow_params(Handle<Shader> shader, Handle<ShaderConfig> config, World& world, RenderParams& params) {
	auto bind_to = params.command_buffer->next_texture_index();
	texture::bind_to(shadow_mask_map, bind_to);
	shader::set_int(shader, "shadowMaskMap", bind_to, config);
}

void ShadowPass::set_shadow_params(Handle<Shader> shader, Handle<ShaderConfig> config, World& world, RenderParams& params) {
	this->shadow_mask.set_shadow_params(shader, config, world, params);
}

struct OrthoProjInfo {
	float endClipSpace;
	glm::mat4 toLight;
	glm::mat4 toWorld;
	glm::mat4 lightProjection;
	glm::mat4 round;
};

constexpr int num_cascades = 4;

void calc_ortho_proj(RenderParams& params, glm::mat4& light_m, float width, float height, OrthoProjInfo shadowOrthoProjInfo[num_cascades]) {
	auto& cam_m = params.view;
	auto cam_inv_m = glm::inverse(cam_m);
	auto& proj_m = params.projection;

	if (!params.cam) throw "Can not calculate cascades without camera";

	float cascadeEnd[] = {
		params.cam->near_plane,
		5.0f,
		30.0f,
		90.0f,
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

		glm::vec4 frustumCorners[8] = {
			glm::vec4(1,1,1,1), //back frustum
			glm::vec4(-1,1,1,1),
			glm::vec4(1,-1,1,1),
			glm::vec4(-1,-1,1,1),

			glm::vec4(1,1,-1,1), //front frustum
			glm::vec4(-1,1,-1,1),
			glm::vec4(1,-1,-1,1),
			glm::vec4(-1,-1,-1,1),
		};

		//modified https://www.gamedev.net/forums/topic/673197-cascaded-shadow-map-shimmering-effect/
		//CONVERT TO WORLD SPACE
		for (int j = 0; j < 8; j++) {
			auto vW = frust_to_world * frustumCorners[j];
			vW /= vW.w;

			frustumCorners[j] = vW;
		}

		glm::vec4 centroid = (frustumCorners[0] + frustumCorners[7]) / 2.0f;

		float radius = glm::length(centroid - frustumCorners[7]);
		for (unsigned int i = 0; i < 8; i++) {
			float distance = glm::length(frustumCorners[i] - centroid);
			radius = glm::max(radius, distance);
		}

		radius = glm::ceil(radius);

		//Create the AABB from the radius
		AABB aabb;
		aabb.max = glm::vec3(centroid) + glm::vec3(radius);
		aabb.min = glm::vec3(centroid) + glm::vec3(radius);

		//aabb.min.z = 2.0f;
		//aabb.max.z = 200.0f;

		aabb.min = glm::vec3(centroid) - glm::vec3(radius);
		aabb.max = glm::vec3(centroid) + glm::vec3(radius);

		aabb.max = glm::vec3(light_m*glm::vec4(aabb.max, 1.0f));
		aabb.min = glm::vec3(light_m*glm::vec4(aabb.min, 1.0f));

		aabb.min.z = 1;
		aabb.max.z = 200;

		GLfloat diagonal_length = glm::length(aabb.max - aabb.min);
		
		// Create the rounding matrix, by projecting the world-space origin and determining
		// the fractional offset in texel space

		// Create the rounding matrix, by projecting the world-space origin and determining
		// the fractional offset in texel space
		
		/*
		glm::mat4 shadowMatrix = lightOrthoMatrix * light_m;
		glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		shadowOrigin = shadowMatrix * shadowOrigin;
		GLfloat storedW = shadowOrigin.w;
		shadowOrigin = shadowOrigin * width / 2.0f;

		glm::vec4 roundedOrigin = glm::round(shadowOrigin);
		glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
		roundOffset = roundOffset * 2.0f / width;
		roundOffset.z = 0.0f;
		roundOffset.w = 0.0f;

		glm::mat4 shadowProj = lightOrthoMatrix;
		shadowProj[3] += roundOffset;
		lightOrthoMatrix = shadowProj;
		*/

		float worldsUnitsPerTexel = (radius * 2.0f) / width;

		aabb.min /= worldsUnitsPerTexel;
		aabb.min = glm::floor(aabb.min);
		aabb.min *= worldsUnitsPerTexel;

		aabb.max /= worldsUnitsPerTexel;
		aabb.max = glm::floor(aabb.max);
		aabb.max *= worldsUnitsPerTexel;

		glm::mat4 lightOrthoMatrix = glm::ortho(aabb.min.x, aabb.max.y, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z);


		glm::vec4 endClipSpace = proj_m * glm::vec4(0, 0, -cascadeEnd[i + 1], 1.0);
		endClipSpace /= endClipSpace.w;

		shadowOrthoProjInfo[i].toWorld = glm::inverse(proj_m * cam_m);
		shadowOrthoProjInfo[i].toLight = lightOrthoMatrix * light_m ;
		shadowOrthoProjInfo[i].lightProjection = lightOrthoMatrix;
		//shadowOrthoProjInfo[i].round = roundMatrix;

		shadowOrthoProjInfo[i].endClipSpace = endClipSpace.z;
	}
}

void DepthMap::render_maps(World& world, RenderParams& params, glm::mat4 projection_m, glm::mat4 view_m, bool is_shadow_pass) {
	RenderParams new_params = params;
	new_params.view = view_m;
	new_params.projection = projection_m;
	new_params.pass = this;

	CommandBuffer cmd_buffer;
	if (is_shadow_pass) {
		new_params.layermask |= shadow_layer;
		new_params.command_buffer = &cmd_buffer;

		Renderer::render_view(world, new_params);
	}

	this->depth_map_FBO.bind();
	this->depth_map_FBO.clear_depth(glm::vec4(0, 0, 0, 1));

	new_params.command_buffer->submit_to_gpu(world, new_params);

	this->depth_map_FBO.unbind();
}

void ShadowPass::render(World& world, RenderParams& params) {
	auto dir_light = get_dir_light(world, params.layermask);
	if (!dir_light) return;
	auto dir_light_id = world.id_of(dir_light);

	auto dir_light_trans = world.by_id<Transform>(dir_light_id);
	if (!dir_light_trans) return;

	glm::mat4 view_matrix = glm::lookAt(dir_light_trans->position, dir_light_trans->position + dir_light->direction, glm::vec3(0, 1, 0));

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
		deffered_map_cascade.render_maps(world, params, proj_info.toLight, glm::mat4(1.0f), true);

		shadow_mask.shadow_mask_map_fbo.bind();

		glDisable(GL_DEPTH_TEST);

		shader::bind(shadow_mask_shader);

		texture::bind_to(depth_prepass, 0);
		shader::set_int(shadow_mask_shader, "depthPrepass", 0);

		texture::bind_to(deffered_map_cascade.depth_map, 1);
		shader::set_int(shadow_mask_shader, "depthMap", 1);

		shader::set_float(shadow_mask_shader, "gCascadeEndClipSpace[0]", last_clip_space);
		shader::set_float(shadow_mask_shader, "gCascadeEndClipSpace[1]", proj_info.endClipSpace);

		shader::set_mat4(shadow_mask_shader, "toWorld", proj_info.toWorld);
		shader::set_mat4(shadow_mask_shader, "toLight", proj_info.toLight);
	
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
		shadow_params.depth_map = deffered_map_cascade.depth_map;

		volumetric.render_with_cascade(world, params, shadow_params);
	}

	//render blur
	bool horizontal = true;
	bool first_iteration = true;

	glDisable(GL_DEPTH_TEST);

	constexpr int amount = 4;

	
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