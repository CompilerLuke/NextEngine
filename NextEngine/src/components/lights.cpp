#include "stdafx.h"
#include "components/lights.h"
#include "ecs/ecs.h"

REFLECT_STRUCT_BEGIN(DirLight)
REFLECT_STRUCT_MEMBER(direction)
REFLECT_STRUCT_MEMBER(color)
REFLECT_STRUCT_END()

DirLight* get_dir_light(World& world, Layermask mask) {
	return world.filter<DirLight>(mask)[0];
}