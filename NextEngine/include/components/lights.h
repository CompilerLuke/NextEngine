#pragma once

#include <glm/vec3.hpp>
#include "ecs/id.h"
#include "core/reflection.h"
#include "ecs/system.h"

REFL
struct DirLight {
	glm::vec3 direction = glm::vec3(0,1.0,0);
	glm::vec3 color = glm::vec3(1.0);
};

REFL
struct PointLight {
	glm::vec3 color;
	float radius;
};

REFL
struct SkyLight {
	bool capture_scene = true;
	cubemap_handle cubemap;
	cubemap_handle irradiance;
	cubemap_handle prefilter;
};

ENGINE_API DirLight* get_dir_light(struct World&, Layermask layermask);