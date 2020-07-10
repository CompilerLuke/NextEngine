#include "components/lights.h"
#include "ecs/ecs.h"
#include "graphics/assets/material.h"
#include "components/transform.h"
#include "components/camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include "components/skybox.h"

DirLight* get_dir_light(World& world, Layermask mask) {
	return &world.first<DirLight>(mask)->get<1>();
}