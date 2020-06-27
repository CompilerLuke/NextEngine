#include "components/lights.h"
#include "ecs/ecs.h"
#include "graphics/assets/material.h"
#include "components/transform.h"
#include "components/camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include "components/skybox.h"

REFLECT_STRUCT_BEGIN(DirLight)
REFLECT_STRUCT_MEMBER(direction)
REFLECT_STRUCT_MEMBER(color)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(PointLight)
REFLECT_STRUCT_MEMBER(color)
REFLECT_STRUCT_MEMBER(radius)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(SkyLight)
REFLECT_STRUCT_MEMBER(cubemap)
REFLECT_STRUCT_MEMBER(capture_scene)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Skybox)
REFLECT_STRUCT_MEMBER(cubemap)
REFLECT_STRUCT_END()

DirLight* get_dir_light(World& world, Layermask mask) {
	return world.filter<DirLight>(mask)[0];
}