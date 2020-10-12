#include "ecs/ecs.h"
#include "graphics/renderer/transforms.h"
#include "components/transform.h"
#include "core/memory/allocator.h"
#include <glm/gtc/matrix_transform.hpp>

void compute_model_matrices(glm::mat4* model_m, World& world, EntityQuery mask) {
	for (auto [e, trans] : world.filter<Transform>(mask)) {
		glm::mat4 identity;

		glm::mat4 translate = glm::translate(identity, trans.position);
		glm::mat4 scale = glm::scale(translate, trans.scale);
		glm::mat4 rotation = glm::mat4_cast(trans.rotation);

		model_m[e.id] = scale * rotation;
	}
}

