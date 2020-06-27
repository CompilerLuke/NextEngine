#pragma once

#include <glm/vec3.hpp>
#include "ecs/id.h"
#include "core/reflection.h"
#include "ecs/system.h"

struct DirLight {
	glm::vec3 direction = glm::vec3(0,1.0,0);
	glm::vec3 color = glm::vec3(1.0);

	REFLECT(ENGINE_API)

};

struct PointLight {
	glm::vec3 color;
	float radius;

	REFLECT(ENGINE_API)
};

struct SkyLight {
	bool capture_scene = true;
	cubemap_handle cubemap;
	cubemap_handle irradiance;
	cubemap_handle prefilter;

	REFLECT(ENGINE_API)
};

ENGINE_API DirLight* get_dir_light(struct World&, Layermask layermask);