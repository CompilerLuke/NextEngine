#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ecs/id.h"

COMP
struct Transform {
	glm::vec3 position = glm::vec3(0.0f);
	glm::quat rotation = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);
};

COMP
struct StaticTransform {
	glm::mat4 model_matrix;
};

COMP
struct LocalTransform {
	glm::vec3 position = glm::vec3(0.0f);
	glm::quat rotation = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);
	ID owner;
};

struct World;
struct UpdateCtx;

ENGINE_API void update_local_transforms(World&, UpdateCtx&);

ENGINE_API void calc_global_transform(World& world, ID id);
ENGINE_API glm::mat4 compute_model_matrix(Transform&);
