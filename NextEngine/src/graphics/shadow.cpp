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

		glm::vec4 farFrustumCorners[8] = {
			glm::vec4(1,1,1,1), //back frustum
			glm::vec4(-1,1,1,1),
			glm::vec4(1,-1,1,1),
			glm::vec4(-1,-1,1,1),
			
			//glm::vec4(0,0,1,1), //centroid

			glm::vec4(1,1,-1,1), //front frustum
			glm::vec4(-1,1,-1,1),
			glm::vec4(1,-1,-1,1),
			glm::vec4(-1,-1,-1,1),
		};

		AABB aabb;

		for (int j = 0; j < 8; j++) {
			auto vW = frust_to_world * farFrustumCorners[j];
			vW /= vW.w;

			farFrustumCorners[j] = light_m * vW;
			
			//aabb.update(farFrustumCorners[j]);
		}

		glm::vec3 centroid;
		float diagonalLength = 0.0f;

		for (int a = 0; a < 8; a++) {
			for (int b = a + 1; b < 8; b++) {
				float new_dist = glm::length(farFrustumCorners[a] - farFrustumCorners[b]);
				new_dist = glm::ceil(new_dist * 10.0f) / 10.0f;

				if (new_dist > diagonalLength) {
					diagonalLength = new_dist;
					centroid = (farFrustumCorners[a] + farFrustumCorners[b]) / 2.0f;
				}
			}
		}

		//float radius = 0.0f;

		float radius = diagonalLength / 2.0f;

		/*
		for (int j = 0; j < 8; j++) {
			auto vW = farFrustumCorners[j];
			//aabb.update(vW);
			radius = glm::max(radius, glm::length(glm::vec3(vW) - centroid));
		}
		*/

		//float radius = glm::length(aabb.max - center); //glm::max(glm::length(aabb.min - center), glm::length(aabb.max - center));

		aabb.min = centroid - radius;
		aabb.max = centroid + radius;
		aabb.min.z = 1;
		aabb.max.z = 200;

		float worldsUnitsPerTexel = diagonalLength / width;

		aabb.min.x = glm::floor(aabb.min.x / worldsUnitsPerTexel) * worldsUnitsPerTexel;
		aabb.min.y = glm::floor(aabb.min.y / worldsUnitsPerTexel) * worldsUnitsPerTexel;

		float viewportExtent = floor((diagonalLength) / worldsUnitsPerTexel) * worldsUnitsPerTexel;    // Ensure view point extents are a texel multiple.  

		aabb.max.x = aabb.min.x + viewportExtent;
		aabb.max.y = aabb.min.y + viewportExtent;

		/*
		glm::mat4 shadow_matrix = glm::ortho(aabb.min.x, aabb.max.y, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z) * light_m;

		glm::vec4 shadowOrigin = shadow_matrix * glm::vec4(glm::vec3(0.0f), 1.0f);
		shadowOrigin /= shadowOrigin.w;

		shadowOrigin *= (width / 2.0f);
		glm::vec2 roundedOrigin = glm::vec2(glm::round(shadowOrigin.x), glm::round(shadowOrigin.y));
		glm::vec2 rounding = roundedOrigin - glm::vec2(shadowOrigin);
		rounding /= (width / 2.0f);

		glm::mat4 roundMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(rounding.x, rounding.y, 0.0f));
		*/

		/*
		glm::vec3 vBorderOffset = (glm::vec3(diagonalLength) - (aabb.max - aabb.min)) * 0.5f;
		aabb.max += vBorderOffset;
		aabb.min -= vBorderOffset;
		*/

		

		/*
		float shadowMapSize = radius * 2.0f;

		float quantizationStep = 1.0f / shadowMapSize;
		float qx = (float)std::remainder(aabb.min.x, quantizationStep);
		float qy = (float)std::remainder(aabb.min.y, quantizationStep);

		aabb.min.x -= qx;
		aabb.min.y -= qy;

		aabb.max.x += shadowMapSize;
		aabb.max.y += shadowMapSize;
		*/

		/*
		glm::vec4 ptOriginShadow(0.0f, 0.0f, 0.0f, 1.0f);
		glm::mat4 light_projection_matrix = glm::ortho(aabb.min.x, aabb.max.y, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z);

		glm::mat4 light_pv = light_projection_matrix * light_m;
		ptOriginShadow = light_pv * ptOriginShadow;

		glm::vec2 texCoord = glm::vec2(ptOriginShadow) * glm::vec2(0.5) * glm::vec2(width, height);
		glm::vec2 texCoordRounded = glm::round(texCoord);

		glm::vec2 d = texCoordRounded - texCoord;

		d /= glm::vec2(width, height) * 0.5f;
	
		glm::mat4 roundMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(d, 0));
		*/

		/*
		float radius = 0;

		//glm::vec4 centroid = farFrustumCorners[4];

		glm::vec3 min = centroid;
		glm::vec3 max = centroid;

		for (int j = 0; j < 9; j++) {
			glm::vec3 corner(farFrustumCorners[j]);

			min = glm::min(min, corner);
			max = glm::max(max, corner);
			//radius = std::max(radius, glm::length(corner - centroid));
		}

		float minX = centroid.x - radius;
		float maxX = centroid.x + radius;
		float minY = centroid.y - radius;
		float maxY = centroid.z + radius;
		float minZ = 1;
		float maxZ = 500; //todo make less hardcoded
		*/

		auto light_projection_matrix = glm::ortho(aabb.min.x, aabb.max.y, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z);


		glm::vec4 endClipSpace = proj_m * glm::vec4(0, 0, -cascadeEnd[i + 1], 1.0);
		endClipSpace /= endClipSpace.w;

		shadowOrthoProjInfo[i].toWorld = glm::inverse(proj_m * cam_m);
		shadowOrthoProjInfo[i].toLight = light_projection_matrix * light_m ;
		shadowOrthoProjInfo[i].lightProjection = light_projection_matrix;
		//shadowOrthoProjInfo[i].round = roundMatrix;

		shadowOrthoProjInfo[i].endClipSpace = endClipSpace.z;
	}
}

void DepthMap::render_maps(World& world, RenderParams& params, glm::mat4 projection_m, glm::mat4 view_m) {
	RenderParams new_params = params;
	new_params.view = view_m;
	new_params.projection = projection_m;
	new_params.pass = this;

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
		deffered_map_cascade.render_maps(world, params, proj_info.toLight, glm::mat4(1.0f));

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