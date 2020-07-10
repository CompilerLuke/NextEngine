#include "graphics/pass/shadow.h"
#include "graphics/assets/assets.h"
#include "graphics/pass/render_pass.h"
#include "components/lights.h"
#include "components/transform.h"
#include "components/camera.h"
#include "graphics/rhi/draw.h"
#include "graphics/rhi/primitives.h"
#include "graphics/renderer/renderer.h"
#include "core/io/logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

DepthMap::DepthMap(unsigned int width, unsigned int height, bool stencil) {
	FramebufferDesc desc{ width, height };	
	add_depth_attachment(desc, &depth_map);
	
	//if (stencil) settings.stencil_buffer = StencilComponent8;
	
	make_Framebuffer(RenderPass::Scene, desc);
	depth_shader = load_Shader("shaders/gizmo.vert", "shaders/depth.frag");
}

ShadowPass::ShadowPass(Renderer& renderer, glm::vec2 size, texture_handle depth_prepass) :
	renderer(renderer),
	deffered_map_cascade(4096, 4096),
	ping_pong_shadow_mask(size),
	shadow_mask(size),
	depth_prepass(depth_prepass),
	volumetric(size * 2.0f, depth_prepass),
	screenspace_blur_shader(load_Shader("shaders/screenspace.vert", "shaders/blur.frag")),
	shadow_mask_shader(load_Shader("shaders/screenspace.vert", "shaders/shadowMask.frag"))
{}

ShadowMask::ShadowMask(glm::vec2 size) {
	FramebufferDesc desc{ size.x, size.y };

	AttachmentDesc& color_attachment = add_color_attachment(desc, &shadow_mask_map);

	make_Framebuffer(RenderPass::Shadow0, desc);
}

void ShadowMask::set_shadow_params(ShaderConfig& config, RenderPass& ctx) {
	//auto bind_to = ctx.command_buffer.next_texture_index();

	//gl_bind_to(assets.textures, shadow_mask_map, bind_to);
	//config.set_int("shadowMaskMap", bind_to);
}

void ShadowPass::set_shadow_params(ShaderConfig& config, RenderPass& params) {
	this->shadow_mask.set_shadow_params(config, params);
}

struct OrthoProjInfo {
	float endClipSpace;
	glm::mat4 toLight;
	glm::mat4 toWorld;
	glm::mat4 lightProjection;
	glm::mat4 round;
};

constexpr int num_cascades = 4;

void calc_ortho_proj(Viewport& viewport, const Camera& camera, glm::mat4& light_m, float width, float height, OrthoProjInfo shadowOrthoProjInfo[num_cascades]) {
	auto& cam_m = viewport.view;
	auto cam_inv_m = glm::inverse(cam_m);
	auto& proj_m = viewport.proj;

	float cascadeEnd[] = {
		camera.near_plane,
		5.0f,
		30.0f,
		90.0f,
		camera.far_plane,
	};

	for (int i = 0; i < num_cascades; i++) {
		auto proj = glm::perspective(
			glm::radians(camera.fov),
			(float)viewport.width / (float)viewport.height,
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
		

		//radius = glm::ceil(radius);

		//Create the AABB from the radius
		AABB aabb;
		aabb.max = glm::vec3(centroid) + glm::vec3(radius);
		aabb.min = glm::vec3(centroid) + glm::vec3(radius);

		aabb.min = glm::vec3(centroid) - glm::vec3(radius);
		aabb.max = glm::vec3(centroid) + glm::vec3(radius);

		aabb.max = glm::vec3(light_m*glm::vec4(aabb.max, 1.0f));
		aabb.min = glm::vec3(light_m*glm::vec4(aabb.min, 1.0f));

		aabb.min.z = 1;
		aabb.max.z = 200;

		float diagonal_length = glm::length(aabb.max - aabb.min);

		//radius = diagonal_length / 2.0f;
		
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
		
		glm::vec3 worldsUnitsPerTexel = (aabb.max - aabb.min) / width; //(radius * 2) / width;

		aabb.min /= worldsUnitsPerTexel;
		aabb.min = glm::floor(aabb.min);
		aabb.min *= worldsUnitsPerTexel;

		aabb.max /= worldsUnitsPerTexel;
		aabb.max = glm::floor(aabb.max);
		aabb.max *= worldsUnitsPerTexel;

		glm::mat4 lightOrthoMatrix = glm::ortho(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z);


		glm::vec4 endClipSpace = proj_m * glm::vec4(0, 0, -cascadeEnd[i + 1], 1.0);
		endClipSpace /= endClipSpace.w;

		shadowOrthoProjInfo[i].toWorld = glm::inverse(proj_m * cam_m);
		shadowOrthoProjInfo[i].toLight = lightOrthoMatrix * light_m ;
		shadowOrthoProjInfo[i].lightProjection = lightOrthoMatrix;
		//shadowOrthoProjInfo[i].round = roundMatrix;

		shadowOrthoProjInfo[i].endClipSpace = endClipSpace.z;
	}
}

/*
void render_scene_to_depth(Renderer& renderer, Framebuffer& depth_map, World& world, RenderPass& ctx, glm::mat4 projection_m, glm::mat4 view_m, RenderPass::PassID pass_id) {
	Viewport viewport = {};
	viewport.width = depth_map_FBO.width;
	viewport.height = depth_map_FBO.height;
	viewport.proj = projection_m;
	viewport.view = view_m;

	//todo render pass, needs to be associated with depth_map_fbo
	RenderPass new_ctx = begin_render_pass(pass_id, RenderPass::Depth_Only, viewport); //ctx, is_shadow_pass ? cmd_buffer : ctx.command_buffer, this);
	
	if (pass_id != RenderPass::Scene) {
		//RENDER VIEW
		//renderer.render_view(world, new_ctx);
	}

	depth_map_FBO.bind();
	depth_map_FBO.clear_depth(glm::vec4(0, 0, 0, 1));


	depth_map_FBO.unbind();

	end_render_pass(new_ctx);
}
*/

struct ShaderUBO {
	glm::mat4 to_world;
	glm::mat4 to_light;
	float end_clip_space[2];
};


void ShadowPass::render(World& world, RenderPass& params) {
	/*auto dir_light = (DirLight*)NULL; //get_dir_light(world, params.layermask);
	if (!dir_light) return;
	auto dir_light_id = world.id_of(dir_light);

	auto dir_light_trans = world.by_id<Transform>(dir_light_id);
	if (!dir_light_trans) return;

	glm::mat4 view_matrix = glm::lookAt(dir_light_trans->position, dir_light_trans->position + dir_light->direction, glm::vec3(0, 1, 0));

	auto width = this->deffered_map_cascade.depth_map_FBO.width;
	auto height = this->deffered_map_cascade.depth_map_FBO.height;

	OrthoProjInfo info[num_cascades];
	//calc_ortho_proj(params, view_matrix, (float)width, (float)height, info);

	//shadow_mask.shadow_mask_map_fbo.bind();
	//shadow_mask.shadow_mask_map_fbo.clear_color(glm::vec4(0, 0, 0, 1));

	SamplerDesc sampler_desc;
	sampler_desc.mag_filter = Filter::Linear;
	sampler_desc.min_filter = Filter::Linear;
	sampler_desc.wrap_u = Wrap::Repeat;
	sampler_desc.wrap_v = Wrap::Repeat;

	float last_clip_space = -1.0f;

	this->volumetric.clear();

	for (int i = 0; i < 4; i++) {
		auto& proj_info = info[i];

		//deffered_map_cascade.id = (RenderPass::PassID)(RenderPass::Shadow0 + i);
		//deffered_map_cascade.render_maps(renderer, world, params, proj_info.toLight, glm::mat4(1.0f), true);

		//shadow_mask.shadow_mask_map_fbo.bind();

		//glDisable(GL_DEPTH_TEST);

		Shader* shadow_mask_shader = assets.shaders.get(this->shadow_mask_shader);

		shadow_mask_shader->bind();

		gl_bind_to(assets.textures, depth_prepass, 0);
		shadow_mask_shader->set_int("depthPrepass", 0);

		gl_bind_to(assets.textures, deffered_map_cascade.depth_map, 1);
		shadow_mask_shader->set_int("depthMap", 1);

		shadow_mask_shader->set_float("gCascadeEndClipSpace[0]", last_clip_space);
		shadow_mask_shader->set_float("gCascadeEndClipSpace[1]", proj_info.endClipSpace);

		shadow_mask_shader->set_mat4("toWorld", proj_info.toWorld);
		shadow_mask_shader->set_mat4("toLight", proj_info.toLight);
	
		glm::mat4 ident_matrix(1.0);
		shadow_mask_shader->set_mat4("model", ident_matrix);

		shadow_mask_shader->set_int("cascadeLevel", i);

		glm::vec2 in_range(last_clip_space, proj_info.endClipSpace);

		last_clip_space = proj_info.endClipSpace;

		//render_quad();

		//glEnable(GL_DEPTH_TEST);

		//shadow_mask.shadow_mask_map_fbo.unbind();

		//compute volumetric scattering for cascade
		ShadowParams shadow_params;
		shadow_params.in_range = in_range;
		shadow_params.to_light = proj_info.toLight;
		shadow_params.to_world = proj_info.toWorld;
		shadow_params.cascade = i;
		shadow_params.depth_map = deffered_map_cascade.depth_map;

		//volumetric.render_with_cascade(world, params, shadow_params);
	}

	//render blur
	bool horizontal = true;
	bool first_iteration = true;

	//glDisable(GL_DEPTH_TEST);

	constexpr int amount = 0;

	
	for (int i = 0; i < amount; i++) {
		//if (!horizontal) shadow_mask.shadow_mask_map_fbo.bind();
		//else ping_pong_shadow_mask.shadow_mask_map_fbo.bind();

		Shader* screenspace_blur_shader = assets.shaders.get(this->screenspace_blur_shader);

		screenspace_blur_shader->bind();
		
		if (first_iteration)
			gl_bind_to(assets.textures, shadow_mask.shadow_mask_map, 0);
		else
			gl_bind_to(assets.textures, ping_pong_shadow_mask.shadow_mask_map, 0);

		screenspace_blur_shader->set_int("image", 0);
		screenspace_blur_shader->set_int("horizontal", horizontal);

		glm::mat4 m(1.0);
		screenspace_blur_shader->set_mat4("model", m);

		//render_quad();

		horizontal = !horizontal;
		first_iteration = false;
		
	}

	//shadow_mask.shadow_mask_map_fbo.unbind();
	//glEnable(GL_DEPTH_TEST);

	*/
}