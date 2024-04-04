#include "graphics/rhi/frame_buffer.h"
#include "graphics/pass/shadow.h"
#include "graphics/assets/assets.h"
#include "components/lights.h"
#include "components/transform.h"
#include "components/camera.h"
#include "graphics/rhi/draw.h"
#include "graphics/rhi/primitives.h"
#include "graphics/renderer/renderer.h"
#include "core/io/logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "ecs/ecs.h"
#include "graphics/culling/culling.h"

// shadow ubo 272

void make_shadow_resources(ShadowResources& resources, UBOBuffer simulation_ubo[MAX_FRAMES_IN_FLIGHT], const ShadowSettings& settings) {
	SamplerDesc shadow_sampler_desc;
	shadow_sampler_desc.mag_filter = Filter::Linear;
	shadow_sampler_desc.min_filter = Filter::Linear;
	shadow_sampler_desc.wrap_u = Wrap::ClampToBorder;
	shadow_sampler_desc.wrap_v = Wrap::ClampToBorder;
	shadow_sampler_desc.depth_compare = true;

	resources.shadow_sampler = query_Sampler(shadow_sampler_desc);

	for (uint frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
		resources.shadow_ubos[frame] = alloc_ubo_buffer(sizeof(ShadowUBO), UBO_PERMANENT_MAP);
	}
	
	for (uint i = 0; i < MAX_SHADOW_CASCADES; i++) {
		FramebufferDesc desc{ settings.shadow_resolution, settings.shadow_resolution };
		add_depth_attachment(desc, &resources.cascade_maps[i]);

		make_Framebuffer((RenderPass::ID)(RenderPass::Shadow0 + i), desc);
		
		for (uint frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
			resources.cascade_ubos[frame][i] = alloc_ubo_buffer(sizeof(PassUBO), UBO_PERMANENT_MAP);
			
			DescriptorDesc desc;
			add_ubo(desc, VERTEX_STAGE, resources.cascade_ubos[frame][i], 0);
			add_ubo(desc, VERTEX_STAGE | FRAGMENT_STAGE, simulation_ubo[frame], 1);

			update_descriptor_set(resources.cascade_descriptors[frame][i], desc);
		}
	}

	resources.cascade_layout = query_Layout(resources.cascade_descriptors[0][0]);
}

void add_shadow_descriptors(DescriptorDesc& desc, ShadowResources& shadow, uint frame) {
	CombinedSampler* cascade_samplers = TEMPORARY_ARRAY(CombinedSampler, MAX_SHADOW_CASCADES);
	for (uint i = 0; i < MAX_SHADOW_CASCADES; i++) {
		cascade_samplers[i] = { shadow.shadow_sampler, shadow.cascade_maps[i] };
	}

	add_ubo(desc, FRAGMENT_STAGE, shadow.shadow_ubos[frame], 4);
	add_combined_sampler(desc, FRAGMENT_STAGE, { cascade_samplers, MAX_SHADOW_CASCADES }, 5);
}

glm::mat4 calc_dir_light_view_matrix(const Transform& trans, const DirLight& dir_light) {
	return glm::lookAt(trans.position, trans.position + dir_light.direction, glm::vec3(0, 1, 0));
}

//modified https://www.gamedev.net/forums/topic/673197-cascaded-shadow-map-shimmering-effect/

//for (uint i = 0; i < 8; i++) {
//	assert(glm::min(aabb.min, glm::vec3(frustumCorners[i])) == aabb.min);
//	assert(glm::max(aabb.max, glm::vec3(frustumCorners[i])) == aabb.max);
//}

void calc_shadow_cascades(ShadowCascadeProj cascades[MAX_SHADOW_CASCADES], const ShadowSettings& settings, Viewport viewports[MAX_SHADOW_CASCADES], const Camera& camera, glm::vec3 lightDir, Viewport& viewport) {
	float nearClip = camera.near_plane;
	float farClip = camera.far_plane;
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	float cascadeSplitLambda = settings.cascade_split_lambda;

	float cascadeSplits[MAX_SHADOW_CASCADES];

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < MAX_SHADOW_CASCADES; i++) {
		float p = (i + 1) / static_cast<float>(MAX_SHADOW_CASCADES);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < MAX_SHADOW_CASCADES; i++) {
        float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		// Project frustum corners into world space
		glm::mat4 invCam = glm::inverse(viewport.proj * viewport.view);
		for (uint32_t i = 0; i < 8; i++) {
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
			frustumCorners[i] = invCorner / invCorner.w;
		}

		for (uint32_t i = 0; i < 4; i++) {
			glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
			frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
			frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t i = 0; i < 8; i++) {
			frustumCenter += frustumCorners[i];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t i = 0; i < 8; i++) {
			float distance = glm::length(frustumCorners[i] - frustumCenter);
			radius = glm::max(radius, distance);
		}

		int round = (MAX_SHADOW_CASCADES - i);
		radius = std::ceil(radius * round) / round;

		//float world_space_per_texel = radius*2 / settings.shadow_resolution;
		//glm::vec3 frustumCenterSnapped = frustumCenter / world_space_per_texel;
		//frustumCenterSnapped = glm::floor(frustumCenterSnapped);
		//frustumCenterSnapped *= world_space_per_texel;
		
		//frustumCenter

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

        minExtents.z = -30;
        maxExtents.z = 30;

		//maxExtents += (frustumCenterSnapped - frustumCenter);
		//minExtents += (frustumCenterSnapped - frustumCenter);

		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

		glm::vec3 ndc_per_texel = glm::vec3(2.0, 2.0, 1.0) / (float)settings.shadow_resolution;
		glm::vec4 quantOrigin = lightOrthoMatrix * lightViewMatrix * glm::vec4(0, 0, 0, 1.0);
		glm::vec3 quantOriginRounded = quantOrigin;
		quantOriginRounded /= ndc_per_texel;
		quantOriginRounded = glm::floor(quantOriginRounded);
		quantOriginRounded *= ndc_per_texel;

		lightOrthoMatrix[3].x += quantOriginRounded.x - quantOrigin.x;
		lightOrthoMatrix[3].y += quantOriginRounded.y - quantOrigin.y;
		lightOrthoMatrix[3].z += quantOriginRounded.z - quantOrigin.z;

		float endDistance = splitDist * clipRange;
		glm::vec4 endClipSpace = viewport.proj * glm::vec4(0, 0, -endDistance, 1.0);
		endClipSpace /= endClipSpace.w;

		//cascades[i].splitDepth = (camera.getNearClip() + splitDist * clipRange) * -1.0f;

		// Store split distance and matrix in cascade
		//cascades[i].end_clip_space = splitDist; // (splitDist * clipRange) * -1.0f;

		//cascades[i].ndc_to_light = glm::inverse(proj_m * cam_m);
		cascades[i].to_light = lightOrthoMatrix * lightViewMatrix;
		cascades[i].light_projection = lightOrthoMatrix;
		cascades[i].light_view = lightViewMatrix;
		cascades[i].end_clip_space = endClipSpace.z;
		cascades[i].end_distance = endDistance;

		viewports[i].proj = lightOrthoMatrix;
		viewports[i].view = lightViewMatrix;
		viewports[i].cam_pos = frustumCenter;
		extract_planes(viewports[i]);

		lastSplitDist = splitDist;
	}
}

void extract_shadow_cascades(ShadowCascadeProj cascades[MAX_SHADOW_CASCADES], Viewport viewports[], const ShadowSettings& settings, World& world, Viewport& viewport, EntityQuery query) {
	auto some_camera = world.first<Camera>(query);
	auto some_dir_light = world.first<Transform, DirLight>();
	if (some_camera && some_dir_light) {
		auto [e1, camera] = *some_camera;
		auto [e2, dir_light_trans, dir_light] = *some_dir_light;
		glm::mat4 light_m = calc_dir_light_view_matrix(dir_light_trans, dir_light);

		calc_shadow_cascades(cascades, settings, viewports, camera, dir_light.direction, viewport);
	}
	
}

void fill_shadow_ubo(ShadowUBO& shadow_ubo, const ShadowCascadeProj info[MAX_SHADOW_CASCADES]) {
	for (uint i = 0; i < MAX_SHADOW_CASCADES; i++) {
		shadow_ubo.cascade_end_clipspace[i].x = info[i].end_clip_space;
		shadow_ubo.cascade_end_clipspace[i].y = info[i].end_distance;
		shadow_ubo.to_light_space[i] = info[i].to_light;
	}
}

void bind_cascade_viewports(ShadowResources& resources, RenderPass render_pass[MAX_SHADOW_CASCADES], const ShadowSettings& settings, const ShadowCascadeProj info[MAX_SHADOW_CASCADES]) {
	uint frame_index = get_frame_index();
	
	for (uint i = 0; i < MAX_SHADOW_CASCADES; i++) {
		PassUBO pass_ubo = {};
		pass_ubo.resolution = glm::vec4(settings.shadow_resolution);
		pass_ubo.proj = info[i].light_projection;
		pass_ubo.view = info[i].light_view;

		memcpy_ubo_buffer(resources.cascade_ubos[frame_index][i], &pass_ubo);

		CommandBuffer& cmd_buffer = *render_pass[i].cmd_buffer;
		bind_pipeline_layout(cmd_buffer, resources.cascade_layout);
		bind_descriptor(cmd_buffer, 0, resources.cascade_descriptors[frame_index][i]);
		set_depth_bias(cmd_buffer, settings.constant_depth_bias, settings.slope_depth_bias);
	}
}
