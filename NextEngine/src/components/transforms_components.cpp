#include "components/transform.h"

#define GLM_FORCE_SSE2 // or GLM_FORCE_SSE42 if your processor supports it
#define GLM_FORCE_ALIGNED

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/simd/matrix.h>

REFLECT_STRUCT_BEGIN(Transform)
REFLECT_STRUCT_MEMBER(position)
REFLECT_STRUCT_MEMBER(rotation)
REFLECT_STRUCT_MEMBER(scale)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(LocalTransform)
REFLECT_STRUCT_MEMBER(position)
REFLECT_STRUCT_MEMBER(rotation)
REFLECT_STRUCT_MEMBER(scale)
REFLECT_STRUCT_MEMBER(owner)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(StaticTransform)
REFLECT_STRUCT_END()

glm::mat4 Transform::compute_model_matrix() {
	glm::mat4 identity(1.0);
	return glm::translate(identity, this->position) * glm::scale(identity, this->scale) * glm::mat4_cast(this->rotation);
}

void LocalTransform::calc_global_transform(World& world) {
	auto owner_local_trans = world.by_id<LocalTransform>(owner);
	if (owner_local_trans) owner_local_trans->calc_global_transform(world);

	auto owner_trans = world.by_id<Transform>(owner);
	if (!owner_trans) return;

	auto trans = world.by_id<Transform>(world.id_of(this));
	if (!trans) return;

	trans->scale = owner_trans->scale * this->scale;
	trans->rotation = owner_trans->rotation * this->rotation;
	auto position = owner_trans->rotation * this->position;
	trans->position = owner_trans->position + position;
}

void LocalTransformSystem::update(World& world, UpdateCtx& params) {
	for (ID id : world.filter<LocalTransform, Transform>(params.layermask | GAME_LAYER)) {
		world.by_id<LocalTransform>(id)->calc_global_transform(world);
	}
}