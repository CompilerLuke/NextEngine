#pragma once

#include "stdafx.h"
#include "objectGizmo.h"
#include "model/model.h"
#include "components/transform.h"
#include "components/lights.h"
#include "components/camera.h"
#include "components/grass.h"
#include "graphics/rhi.h"
#include "graphics/renderer.h"
#include "graphics/materialSystem.h"

ObjectGizmoSystem::ObjectGizmoSystem() {
	this->dir_light_model = load_Model("editor/dirLight.fbx", true);
	this->grass_model = load_Model("editor/grass.fbx", true);
	this->camera_model = load_Model("editor/camera.fbx", true);

	Material gizmo_mat(load_Shader("shaders/pbr.vert", "shaders/diffuse.frag"));
	gizmo_mat.set_vec3("material.diffuse", glm::vec3(0, 0.8, 0.8));

	this->gizmo_materials.append(RHI::material_manager.make(std::move(gizmo_mat)));
}

void render_gizmo(Handle<Model> model, vector<Handle<Material>> gizmo_materials, World& world, RenderParams& params, ID id) {
	Transform trans = *world.by_id<Transform>(id);

	auto model_m = TEMPORARY_ALLOC(glm::mat4);

	*model_m = trans.compute_model_matrix();

	if (params.layermask & editor_layer || params.layermask & picking_layer) {
		RHI::model_manager.get(model)->render(id, model_m, gizmo_materials, params);
	}
}

void ObjectGizmoSystem::render(World& world, RenderParams& params) {
	for (ID id : world.filter<Grass, Transform>(game_layer)) {
		render_gizmo(grass_model, gizmo_materials, world, params, id);
	}

	for (ID id : world.filter<Camera, Transform>(game_layer)) {
		render_gizmo(camera_model, gizmo_materials, world, params, id);
	}

	for (ID id : world.filter<DirLight, Transform>(game_layer)) {
		auto dir_light = world.by_id<DirLight>(id);
		auto trans = world.by_id<Transform>(id);

		dir_light->direction = trans->rotation * glm::vec3(0, 1, 0);
		dir_light->direction = glm::normalize(dir_light->direction);

		render_gizmo(dir_light_model, gizmo_materials, world, params, id);
	}
}