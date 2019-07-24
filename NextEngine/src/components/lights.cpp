#include "stdafx.h"
#include "components/lights.h"
#include "ecs/ecs.h"
#include "model/model.h"
#include "graphics/materialSystem.h"
#include "graphics/rhi.h"
#include "components/transform.h"

REFLECT_STRUCT_BEGIN(DirLight)
REFLECT_STRUCT_MEMBER(direction)
REFLECT_STRUCT_MEMBER(color)
REFLECT_STRUCT_END()

DirLight* get_dir_light(World& world, Layermask mask) {
	return world.filter<DirLight>(mask)[0];
}

DebugLightSystem::DebugLightSystem() {
	auto shad = load_Shader("shaders/pbr.vert", "shaders/diffuse.frag");

	this->dir_light_model = load_Model("editor/dirLight.fbx");
	this->gizmo_materials = { RHI::material_manager.make({
		"DefaultMaterial",
		shad,
		{
			make_Param_Vec3(location(shad, "material.diffuse"), glm::vec3(0, 0.8, 0.8))
		},
		&default_draw_state
	}) };
}

void DebugLightSystem::render(World& world, RenderParams& params) {

	
	for (ID id : world.filter<DirLight, Transform>(params.layermask)) {
		auto dir_light = world.by_id<DirLight>(id);
		auto trans = world.by_id<Transform>(id);

		dir_light->direction = trans->rotation * glm::vec3( 0,1,0 );
		dir_light->direction = glm::normalize(dir_light->direction);

		auto model_m = TEMPORARY_ALLOC(glm::mat4);
		*model_m = trans->compute_model_matrix();

		if (params.layermask & editor_layer || params.layermask & picking_layer) {
			RHI::model_manager.get(dir_light_model)->render(id, model_m, gizmo_materials, params);
		}
	}
}