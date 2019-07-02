#pragma once

#include <glm/vec3.hpp>
#include "ecs/id.h"
#include "reflection/reflection.h"

struct DirLight {
	glm::vec3 direction = glm::vec3(0,1.0,0);
	glm::vec3 color = glm::vec3(1.0);

	REFLECT()

};

DirLight* get_dir_light(struct World&, Layermask layermask);