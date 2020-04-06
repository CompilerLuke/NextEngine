#include "stdafx.h"
#include "components/lights.h"
#include "ecs/ecs.h"
#include "graphics/renderer/material_system.h"
#include "components/transform.h"
#include "components/camera.h"
#include <glm/gtc/matrix_transform.hpp>

REFLECT_STRUCT_BEGIN(DirLight)
REFLECT_STRUCT_MEMBER(direction)
REFLECT_STRUCT_MEMBER(color)
REFLECT_STRUCT_END()

DirLight* get_dir_light(World& world, Layermask mask) {
	return world.filter<DirLight>(mask)[0];
}