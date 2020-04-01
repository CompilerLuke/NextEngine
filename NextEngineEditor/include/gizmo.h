#pragma once

enum GizmoMode {
	TranslateGizmo, ScaleGizmo, RotateGizmo, DisabledGizmo
};

struct Gizmo {
	GizmoMode mode;
	struct DiffUtil* diff_util = nullptr;

	float snap_amount = 1.0f;
	bool snap;

	void register_callbacks(struct Editor& editor);

	void update(struct World&, struct Editor&, struct UpdateCtx&);
	void render(struct World&, struct Editor&, struct RenderCtx&, struct Input&);

	~Gizmo();
};