#pragma once

#include "core/vector.h"
#include "core/handle.h"
#include "graphics/renderFeature.h"

struct ObjectGizmoSystem : RenderFeature {
	vector<Handle<struct Material>> gizmo_materials;
	Handle<struct Model> dir_light_model;
	Handle<struct Model> grass_model;
	Handle<struct Model> camera_model;

	ObjectGizmoSystem();
	void render(struct World&, RenderParams&) override;
};