#include "stdafx.h"
#include "graphics/renderer/transforms.h"
#include "ecs/layermask.h"
#include "components/transform.h"
#include "core/memory/allocator.h"
#include <glm/gtc/matrix_transform.hpp>

void compute_model_matrices(glm::mat4* model_m, World& world, Layermask mask) {
	for (Transform* trans : world.filter<Transform>(mask)) {
		glm::mat4 identity;

		glm::mat4 translate = glm::translate(identity, trans->position);
		glm::mat4 scale = glm::scale(translate, trans->scale);
		glm::mat4 rotation = glm::mat4_cast(trans->rotation);

		model_m[world.id_of(trans)] = scale * rotation;
	}
}