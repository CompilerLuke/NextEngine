#pragma once

#include "core/container/array.h"
#include "ecs/id.h"
#include "graphics/rhi/buffer.h"
#include "graphics/rhi/shader_access.h"
#include "graphics/pass/pass.h"

struct Skybox;

#define MAX_POINT_LIGHTS 4

struct PointLightUBO {
	glm::vec3 position;
	float radius;
	glm::vec4 color;
};

struct DirLightUBO {
	glm::vec4 direction;
	glm::vec4 color;
};

struct alignas(16) LightUBO {	
	DirLightUBO dir_light;
	PointLightUBO point_lights[MAX_POINT_LIGHTS];
	glm::vec3 viewpos;
	int num_point_lights;
};

struct SkylightFiltered {
	cubemap_handle irradiance_cubemap;
	cubemap_handle prefilter_cubemap;
};

#define MAX_SKYLIGHT_FILTERED 2

struct LightingSystem {
	texture_handle brdf_LUT = { INVALID_HANDLE };
	UBOBuffer light_ubo[MAX_FRAMES_IN_FLIGHT];
	SkylightFiltered skylight_filtered[MAX_SKYLIGHT_FILTERED];
	descriptor_set_handle pbr_descriptor[MAX_FRAMES_IN_FLIGHT];
};

struct RenderPass;
struct LightUBO;
struct SkyLight;
struct World;
struct ShadowResources;

ENGINE_API void make_lighting_system(LightingSystem& system, ShadowResources& shadow, SkyLight& skylight);
void fill_light_ubo(LightUBO& ubo, World& world, Viewport& viewport, EntityQuery mask);
void bind_color_pass_lighting(CommandBuffer& cmd_buffer, LightingSystem& system);