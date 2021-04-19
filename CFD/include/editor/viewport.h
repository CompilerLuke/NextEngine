#pragma once

#include "graphics/pass/pass.h"
#include "engine/input.h"
#include "core/math/vec2.h"
#include "editor/selection.h"

struct SceneViewport {
	enum Mode { Scene, Object };

	Mode mode = Scene;
	Viewport viewport;
	Input input;
	SceneSelection scene_selection;
	InputMeshSelection mesh_selection;

	SceneViewport(struct Window&);
	void update(glm::vec2 offset, glm::vec2 size);
};