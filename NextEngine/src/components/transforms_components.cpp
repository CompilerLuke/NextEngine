#include "components/transform.h"

#define GLM_FORCE_SSE2 // or GLM_FORCE_SSE42 if your processor supports it
#define GLM_FORCE_ALIGNED

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/simd/matrix.h>

glm::mat4 compute_model_matrix(Transform& trans) {
	glm::mat4 identity(1.0);
	return glm::translate(identity, trans.position) * glm::scale(identity, trans.scale) * glm::mat4_cast(trans.rotation);
}

void calc_global_transform(World& world, ID id) {
	LocalTransform* local = world.by_id<LocalTransform>(id);

	auto owner_local_trans = world.by_id<LocalTransform>(local->owner);
	if (owner_local_trans) calc_global_transform(world, local->owner);

	auto owner_trans = world.by_id<Transform>(local->owner);
	if (!owner_trans) return;

	auto trans = world.by_id<Transform>(id);
	if (!trans) return;

	trans->scale = owner_trans->scale * local->scale;
	trans->rotation = owner_trans->rotation * local->rotation;
	auto position = owner_trans->rotation * local->position;
	trans->position = owner_trans->position + position;
}

//todo more efficient system would be to store local transforms by depth, and calculate each level independently
//parrelises nicely too!
void LocalTransformSystem::update(World& world, UpdateCtx& params) {
	for (ID id : world.filter<LocalTransform, Transform>(params.layermask | GAME_LAYER)) {
		calc_global_transform(world, id);
	}
}