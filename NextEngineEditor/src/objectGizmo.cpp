#pragma once

#include "objectGizmo.h"
#include "graphics/assets/assets.h"
#include "components/transform.h"
#include "components/lights.h"
#include "components/camera.h"
#include "components/grass.h"
#include "graphics/renderer/renderer.h"
#include "graphics/renderer/model_rendering.h"

ObjectGizmoSystem::ObjectGizmoSystem() {
	this->dir_light_model =  load_Model("editor/dirLight.fbx");
	this->grass_model =      load_Model("editor/grass.fbx");
	this->camera_model =     load_Model("editor/camera.fbx");

	//Material gizmo_mat(load_Shader(assets, "shaders/pbr.vert", "shaders/diffuse.frag"));
	//gizmo_mat.set_vec3(asset_manager.shaders, "material.diffuse", glm::vec3(0, 0.8, 0.8));

	//this->gizmo_materials.append(asset_manager.materials.assign_handle(std::move(gizmo_mat)));
}

void render_gizmo(model_handle model, slice<material_handle> gizmo_materials, World& world, RenderPass& ctx, ID id) {
	Transform trans = *world.by_id<Transform>(id);
	draw_mesh(ctx.cmd_buffer, model, gizmo_materials, trans);

	//	ctx.command_buffer.draw(model_m, model, gizmo_materials);
}

void ObjectGizmoSystem::render(World& world, RenderPass& ctx) {
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