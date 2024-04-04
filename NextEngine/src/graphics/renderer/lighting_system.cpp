#include "graphics/renderer/lighting_system.h"
#include "components/lights.h"
#include "graphics/renderer/ibl.h"
#include "components/transform.h"
#include "core/container/array.h"
#include "ecs/ecs.h"
#include "graphics/pass/pass.h"

#include "components/camera.h"
#include "graphics/rhi/draw.h"
#include "graphics/pass/shadow.h"

ENGINE_API uint get_frame_index();

//env_probe_handle make_EnvironmentProbe(cubemap_handle cubemap_handle) {
//	EnvProbe env_probe;
//	make_EnvironmentProbe(env_probe, cubemap_handle);
//
//	return assets.env_probes.assign_handle(std::move(env_probe));
//}

void bake_scene_lighting(LightingSystem& lighting_system, World& world, EntityQuery layermask) {
	//for (SkyLight* skylight : world.filter<SkyLight>(layermask)) {
	//	cubemap_handle cubemap_handle = skylight->cubemap;
	//	extract_lighting_from_cubemap(lighting_system, world.component_id(skylight), cubemap_handle);
	//}
}



void make_lighting_system(LightingSystem& system, ShadowResources& shadow, SkyLight& skylight) {
	system.brdf_LUT = compute_brdf_lut(512);

	SamplerDesc linear_sampler_desc;
	linear_sampler_desc.mag_filter = Filter::Linear;
	linear_sampler_desc.min_filter = Filter::Linear;
	linear_sampler_desc.mip_mode = Filter::Linear;
	linear_sampler_desc.wrap_u = Wrap::ClampToBorder;
	linear_sampler_desc.wrap_v = Wrap::ClampToBorder;

	sampler_handle linear_sampler = query_Sampler(linear_sampler_desc);

	for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		system.light_ubo[i] = alloc_ubo_buffer(sizeof(LightUBO), UBO_PERMANENT_MAP);

		DescriptorDesc desc;
		add_ubo(desc, FRAGMENT_STAGE, system.light_ubo[i], 0);

		add_combined_sampler(desc, FRAGMENT_STAGE, linear_sampler, skylight.irradiance, 1);
		add_combined_sampler(desc, FRAGMENT_STAGE, linear_sampler, skylight.prefilter, 2);
		add_combined_sampler(desc, FRAGMENT_STAGE, linear_sampler, system.brdf_LUT, 3);

		add_shadow_descriptors(desc, shadow, i);

		update_descriptor_set(system.pbr_descriptor[i], desc);
	}
}

void fill_light_ubo(LightUBO& light_ubo, World& world, Viewport& viewport, EntityQuery mask) {
	light_ubo = {};
	light_ubo.viewpos = viewport.cam_pos;

	for (auto [e, trans, point_light] : world.filter<Transform, PointLight>(mask)) {
		PointLightUBO& ubo = light_ubo.point_lights[light_ubo.num_point_lights++];
		ubo.position = trans.position;
		ubo.color = glm::vec4(point_light.color, 1.0);
		ubo.radius = point_light.radius;

		if (light_ubo.num_point_lights == MAX_POINT_LIGHTS) break;
	}

	for (auto [e,dir_light] : world.filter<DirLight>(mask)) {
		DirLightUBO ubo = {};
		ubo.direction = glm::vec4(dir_light.direction, 1.0);
		ubo.color = glm::vec4(dir_light.color, 1.0);
	
		light_ubo.dir_light = ubo;
		break;
	}
}

void bind_color_pass_lighting(CommandBuffer& cmd_buffer, LightingSystem& system) {
	bind_descriptor(cmd_buffer, 2, system.pbr_descriptor[get_frame_index()]);
}
