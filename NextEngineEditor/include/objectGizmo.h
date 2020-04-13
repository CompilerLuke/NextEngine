#pragma once

#include "core/container/vector.h"
#include "core/handle.h"
#include "graphics/renderer/render_feature.h"

struct ModelRendererSystem;
struct Assets;
struct World;

struct ObjectGizmoSystem : RenderFeature {
	Assets& asset_manager;
	
	vector<material_handle> gizmo_materials;
	model_handle dir_light_model;
	model_handle grass_model;
	model_handle camera_model;

	ObjectGizmoSystem(Assets& asset_manager);
	void render(World&, RenderCtx&) override;
};