#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ecs/ecs.h"
#include "core/reflection.h"

COMP
struct Transform {
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale = glm::vec3(1.0f);
};

COMP
struct StaticTransform {
	glm::mat4 model_matrix;
};

COMP
struct LocalTransform {
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale = glm::vec3(1.0f);
	ID owner;
};

struct LocalTransformSystem : System {
	void update(World&, UpdateCtx&) override;
};



ENGINE_API void calc_global_transform(World& world, ID id);
ENGINE_API glm::mat4 compute_model_matrix(Transform&);