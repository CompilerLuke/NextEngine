#pragma once

enum GizmoMode {
	TranslateGizmo, ScaleGizmo, RotateGizmo, DisabledGizmo
};

struct Gizmo {
	GizmoMode mode;
	struct DiffUtil* diff_util = NULL;

	float snap_amount = 1.0f;
	bool snap;

	void update(struct World&, struct Editor&, struct UpdateParams&);
	void render(struct World&, struct Editor&, struct RenderParams&);

	~Gizmo();
};