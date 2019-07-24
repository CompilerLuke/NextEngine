#pragma once

#include "imgui.h"
#include "core/vector.h"
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <string>
#include <glm/mat4x4.hpp>
#include "reflection/reflection.h"
#include "core/string_view.h"

struct DisplayComponents {
	vector<float> fps_times;

	void update(struct World& world, struct UpdateParams& params);
	void render(struct World& world, struct RenderParams& params, struct Editor& editor);
};

bool render_fields_primitive(int*, StringView);
bool render_fields_primitive(unsigned int*, StringView);
bool render_fields_primitive(float*, StringView);
bool render_fields_primitive(StringBuffer*, StringView);
bool render_fields_primitive(bool*, StringView);

bool render_fields(reflect::TypeDescriptor*, void*, StringView, World&);

using OnInspectGUICallback = bool(*)(void*, StringView, World&);

void register_on_inspect_gui(StringView, OnInspectGUICallback);
OnInspectGUICallback get_on_inspect_gui(StringView on_type);