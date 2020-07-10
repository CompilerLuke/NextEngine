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

void calc_global_transform(World& world, LocalTransform& local, Transform& trans) {
	auto owner = world.get_by_id<LocalTransform, Transform>(local.owner);
	if (!owner) return;

	auto[owner_local_trans, owner_trans] = *owner;

	trans.scale = owner_trans.scale * local.scale;
	trans.rotation = owner_trans.rotation * local.rotation;
	auto position = owner_trans.rotation * local.position;
	trans.position = owner_trans.position + position;
}


void calc_global_transform(World& world, ID id) {
	auto[local, trans] = *world.get_by_id<LocalTransform, Transform>(id);
	calc_global_transform(world, local, trans);
}

//todo more efficient system would be to store local transforms by depth, and calculate each level independently
//parrelises nicely too!
void LocalTransformSystem::update(World& world, UpdateCtx& params) {
	for (auto [e, local, trans] : world.filter<LocalTransform, Transform>(params.layermask | GAME_LAYER)) {
		calc_global_transform(world, local, trans);
	}
}