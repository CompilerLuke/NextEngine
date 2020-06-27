#pragma once

#include "ecs/layermask.h"
#include "core/container/array.h"
#include "core/container/handle_manager.h"
#include "graphics/rhi/buffer.h"
#include "graphics/rhi/shader_access.h"

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
	UBOBuffer light_ubo;
	SkylightFiltered skylight_filtered[MAX_SKYLIGHT_FILTERED];
	descriptor_set_handle pbr_descriptor;
};

struct RenderPass;
struct LightUBO;
struct SkyLight;

ENGINE_API void make_lighting_system(LightingSystem& system, SkyLight& skylight);
void fill_light_ubo(LightUBO& ubo, World& world, Layermask mask);
void bind_color_pass_lighting(CommandBuffer& cmd_buffer, LightingSystem& system);