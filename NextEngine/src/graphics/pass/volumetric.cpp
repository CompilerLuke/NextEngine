#include "graphics/pass/volumetric.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/primitives.h"
#include "graphics/rhi/frame_buffer.h"
#include "graphics/rhi/pipeline.h"

#include "components/lights.h"
#include "components/transform.h"
#include "components/camera.h"
#include "core/io/logger.h"
#include "graphics/pass/shadow.h"
#include "graphics/pass/composite.h"
#include "ecs/ecs.h"

#include "core/time.h"

ENGINE_API uint get_frame_index();

void make_volumetric_resources(VolumetricResources& resources, texture_handle depth_prepass, ShadowResources& shadow, uint width, uint height) {
	resources.volume_shader = load_Shader("shaders/screenspace.vert", "shaders/volumetric.frag");
	
	FramebufferDesc desc{ width, height };
	AttachmentDesc& volumetric_lighting = add_color_attachment(desc, &resources.volumetric_map);
	volumetric_lighting.format = TextureFormat::HDR;

	AttachmentDesc& cloud_lighting = add_color_attachment(desc, &resources.cloud_map);
	cloud_lighting.format = TextureFormat::HDR;

	add_dependency(desc, FRAGMENT_STAGE, RenderPass::Scene, TextureAspect::Depth);

	make_Framebuffer(RenderPass::Volumetric, desc);

	sampler_handle depthprepass_sampler = query_Sampler({});

	for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		resources.ubo[i] = alloc_ubo_buffer(sizeof(VolumetricUBO), UBO_PERMANENT_MAP);

		DescriptorDesc desc;
		add_ubo(desc, FRAGMENT_STAGE, resources.ubo[i], 0);
		add_combined_sampler(desc, FRAGMENT_STAGE, depthprepass_sampler, depth_prepass, 1);
		add_shadow_descriptors(desc, shadow, i);

		update_descriptor_set(resources.descriptor[i], desc);
	}

	GraphicsPipelineDesc pipeline_desc;
	pipeline_desc.shader = resources.volume_shader;
	pipeline_desc.render_pass = RenderPass::Volumetric;

	resources.pipeline = query_Pipeline(pipeline_desc);
}

void fill_volumetric_ubo(VolumetricUBO& ubo, CompositeUBO& composite, World& world, VolumetricSettings& settings, Viewport& viewport, EntityQuery query) {
	ubo = {};
	
	auto some_camera = world.first<Transform, Camera>(query); //todo looked up redundantly
	auto some_light = world.first<Transform, DirLight>();
	auto some_fog = world.first<Transform, FogVolume>();
	auto some_cloud = world.first<Transform, CloudVolume>();

	if (some_camera && some_light && (some_fog || some_cloud)) {
		auto [e1, cam_trans, cam] = *some_camera;
		auto [e2, light_trans, light] = *some_light;

		ubo.to_world = glm::inverse(viewport.proj * viewport.view);
		ubo.cam_position = cam_trans.position;
		ubo.sun_direction = glm::normalize(light.direction);
		ubo.sun_color = light.color;
		ubo.sun_position = light_trans.position;

		if (some_fog) {
			auto [e3, trans, fog] = *some_fog;
			ubo.fog_steps = settings.fog_steps;
			ubo.scatterColor = fog.forward_scatter_color;
			ubo.fog_coefficient = fog.coefficient;
			ubo.intensity = fog.intensity;
            
            composite.fog_color = fog.fog_color;
            composite.fog_begin = fog.fog_begin;
		}

		if (some_cloud) {
			auto [e4, trans, cloud] = *some_cloud;
			ubo.cloud_steps = settings.cloud_steps;
			ubo.cloud_shadow_steps = settings.cloud_shadow_steps;
			ubo.fog_cloud_shadow_steps = settings.fog_cloud_shadow_steps;
			
			ubo.light_absorbtion_in_cloud = cloud.light_absorbtion_in_cloud;
			ubo.light_absorbtion_towards_sun = cloud.light_absorbtion_towards_sun;
			ubo.forward_intensity = cloud.forward_scatter_intensity;
			ubo.cloud_phase = cloud.phase;
			ubo.shadow_darkness_threshold = cloud.shadow_darkness_threshold;
			ubo.cloud_bottom = trans.position.y - cloud.size.y;
			ubo.cloud_top = trans.position.y + cloud.size.y;
			ubo.wind = cloud.wind;
		}

		ubo.time = Time::now();
	}
}

void render_volumetric_pass(VolumetricResources& resources, const VolumetricUBO& ubo) {
	if (ubo.fog_steps == 0 && ubo.cloud_steps == 0) {
		RenderPass render_pass = begin_render_pass(RenderPass::Volumetric, glm::vec4(0,0,0,1));
		end_render_pass(render_pass);
		return;
	}
	
	uint frame_index = get_frame_index();
	memcpy_ubo_buffer(resources.ubo[frame_index], &ubo);

	RenderPass render_pass = begin_render_pass(RenderPass::Volumetric);
	CommandBuffer& cmd_buffer = *render_pass.cmd_buffer;
    bind_pipeline(cmd_buffer, resources.pipeline);
	bind_descriptor(cmd_buffer, 1, resources.descriptor[frame_index]);
	draw_quad(cmd_buffer);

	end_render_pass(render_pass);
}
