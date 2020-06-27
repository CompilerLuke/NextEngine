#pragma once

#include "core/handle.h"
#include "core/container/sstring.h"
#include "core/io/input.h"
#include "graphics/pass/pass.h"

enum EditorViewportType {
	EDITOR_VIEWPORT_TYPE_GAME,
	EDITOR_VIEWPORT_TYPE_SCENE
};

enum EditorViewportFlags {
	EDITOR_VIEWPORT_VISIBILITY_SHOW_CAMERAS,
	EDITOR_VIEWPORT_VISIBILITY_SHOW_LIGHTS,
	EDITOR_VIEWPORT_VISIBILITY_SHOW_COLLIDERS,
};

inline EditorViewportFlags operator|(EditorViewportFlags self, EditorViewportFlags other) {
	return (EditorViewportFlags)((uint)self | (uint)other);
}

inline void operator|=(EditorViewportFlags& self, EditorViewportFlags other) {
	self = self | other;
}

struct EditorViewport {
	sstring name;
	EditorViewportType type;
	EditorViewportFlags flags;

	texture_handle contents;
	Viewport viewport;

	Input input;
};