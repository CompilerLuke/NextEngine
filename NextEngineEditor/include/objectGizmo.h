#pragma once

#include "core/container/vector.h"
#include "engine/handle.h"
#include "graphics/renderer/render_feature.h"

struct ModelRendererSystem;
struct Assets;
struct World;

struct ObjectGizmoSystem : RenderFeature {	
	vector<material_handle> gizmo_materials;
	model_handle dir_light_model;
	model_handle grass_model;
	model_handle camera_model;

	ObjectGizmoSystem();
	void render(World&, RenderPass&) override;
};