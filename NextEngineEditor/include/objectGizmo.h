#pragma once

#include "core/container/vector.h"
#include "core/handle.h"
#include "graphics/renderer/render_feature.h"

struct ModelRendererSystem;
struct AssetManager;

struct ObjectGizmoSystem : RenderFeature {
	AssetManager& asset_manager;
	ModelRendererSystem& model_renderer;
	
	vector<material_handle> gizmo_materials;
	model_handle dir_light_model;
	model_handle grass_model;
	model_handle camera_model;

	ObjectGizmoSystem(ModelRendererSystem& model_renderer);
	void render(struct World&, RenderCtx&) override;
};