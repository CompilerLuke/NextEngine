#pragma once

#include "engine/handle.h"
#include "diffUtil.h"
#include "components/transform.h"

struct GizmoResources {
	model_handle camera_model;

	material_handle terrain_control_point_material;
	material_handle terrain_splat_material;
	material_handle camera_material;
	material_handle point_material;
	material_handle dir_light_material;
	material_handle grass_system_material;
	material_handle particle_system_material;
};

struct GizmoRenderData {
	tvector<glm::mat4> terrain_control_points;
	tvector<glm::mat4> terrain_splats;
	tvector<glm::mat4> cameras;
	tvector<glm::mat4> point_lights;
	tvector<glm::mat4> dir_lights;
	tvector<glm::mat4> grass_system;
	tvector<glm::mat4> particle_system;
};

enum class GizmoMode {
	Translate, Scale, Rotate, Disabled
};

struct Gizmo {
	GizmoMode mode;
	Transform copy_transform;
	DiffUtil diff_util;

	float snap_amount = 1.0f;
	bool snap;

	void register_callbacks(struct Editor& editor);

	void update(struct World&, Editor&, struct UpdateCtx&);
	void render(struct World&, Editor&, struct Viewport&, struct Input&);

	~Gizmo();
};

void make_special_gizmo_resources(GizmoResources& resources);
void extract_render_data_special_gizmo(GizmoRenderData& render_data, World& world, EntityQuery query);
void render_special_gizmos(GizmoResources& resources, const GizmoRenderData& render_data, struct RenderPass& render_pass);
