#pragma once

#include "stdafx.h"
#include "objectGizmo.h"
#include "graphics/assets/asset_manager.h"
#include "components/transform.h"
#include "components/lights.h"
#include "components/camera.h"
#include "components/grass.h"
#include "graphics/renderer/renderer.h"
#include "graphics/renderer/model_rendering.h"

ObjectGizmoSystem::ObjectGizmoSystem(AssetManager& asset_manager) 
: asset_manager(asset_manager) {
	this->dir_light_model =  asset_manager.models.load("editor/dirLight.fbx", true);
	this->grass_model =      asset_manager.models.load("editor/grass.fbx", true);
	this->camera_model =     asset_manager.models.load("editor/camera.fbx", true);

	Material gizmo_mat(asset_manager.shaders.load("shaders/pbr.vert", "shaders/diffuse.frag"));
	gizmo_mat.set_vec3(asset_manager.shaders, "material.diffuse", glm::vec3(0, 0.8, 0.8));

	this->gizmo_materials.append(asset_manager.materials.assign_handle(std::move(gizmo_mat)));
}

void render_gizmo(model_handle model, slice<material_handle> gizmo_materials, World& world, RenderCtx& ctx, ID id) {
	Transform trans = *world.by_id<Transform>(id);

	glm::mat4 model_m = model_m = trans.compute_model_matrix();

	if (ctx.layermask & EDITOR_LAYER || ctx.layermask & PICKING_LAYER) {
		ctx.command_buffer.draw(model_m, model, gizmo_materials);
	}
}

void ObjectGizmoSystem::render(World& world, RenderCtx& ctx) {
	for (ID id : world.filter<Grass, Transform>(GAME_LAYER)) {
		render_gizmo(grass_model, gizmo_materials, world, ctx, id);
	}

	for (ID id : world.filter<Camera, Transform>(GAME_LAYER)) {
		render_gizmo(camera_model, gizmo_materials, world, ctx, id);
	}

	for (ID id : world.filter<DirLight, Transform>(GAME_LAYER)) {
		auto dir_light = world.by_id<DirLight>(id);
		auto trans = world.by_id<Transform>(id);

		dir_light->direction = trans->rotation * glm::vec3(0, 1, 0);
		dir_light->direction = glm::normalize(dir_light->direction);

		render_gizmo(dir_light_model, gizmo_materials, world, ctx, id);
	}
}