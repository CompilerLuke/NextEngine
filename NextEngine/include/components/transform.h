#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ecs/ecs.h"
#include "reflection/reflection.h"

struct Transform {
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale = glm::vec3(1.0f);
	
	glm::mat4 ENGINE_API compute_model_matrix();

	REFLECT()
};

struct StaticTransform {
	glm::mat4 model_matrix;

	REFLECT()
};

struct LocalTransform {
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale = glm::vec3(1.0f);
	ID owner;

	void ENGINE_API calc_global_transform(struct World&);

	REFLECT()
};

struct LocalTransformSystem : System {
	void update(World&, UpdateParams&) override;
};

struct TransformSystem : System {
	void pre_render(World&, PreRenderParams&) override;
};