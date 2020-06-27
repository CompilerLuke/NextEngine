#pragma once

#include "diffUtil.h"
#include "components/transform.h"

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

	void update(struct World&, struct Editor&, struct UpdateCtx&);
	void render(struct World&, struct Editor&, struct Viewport&, struct Input&);

	~Gizmo();
};