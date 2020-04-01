#pragma once

#include "core/container/vector.h"
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "core/reflection.h"
#include "core/container/string_view.h"
#include "core/container/string_buffer.h"

struct DisplayComponents {
	string_buffer filter;

	void update(struct World& world, struct UpdateCtx& params);
	void render(struct World& world, struct RenderCtx& params, struct Editor& editor);
};

bool render_fields_primitive(int*, string_view);
bool render_fields_primitive(unsigned int*, string_view);
bool render_fields_primitive(float*, string_view);
bool render_fields_primitive(string_buffer*, string_view);
bool render_fields_primitive(bool*, string_view);

bool render_fields(reflect::TypeDescriptor*, void*, string_view, World&);

using OnInspectGUICallback = bool(*)(void*, string_view, World&);

void register_on_inspect_gui(string_view, OnInspectGUICallback);
OnInspectGUICallback get_on_inspect_gui(string_view on_type);