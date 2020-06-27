#pragma once

#include "core/container/vector.h"
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "core/reflection.h"
#include "core/container/string_view.h"
#include "core/container/string_buffer.h"

struct World;
struct Editor;
struct UpdateCtx;
struct RenderPass;

struct DisplayComponents {
	string_buffer filter;

	void update(World& world, UpdateCtx& params);
	void render(World& world, RenderPass& params, struct Editor& editor);
};

bool render_fields_primitive(int*, string_view);
bool render_fields_primitive(unsigned int*, string_view);
bool render_fields_primitive(float*, string_view);
bool render_fields_primitive(string_buffer*, string_view);
bool render_fields_primitive(bool*, string_view);

bool render_fields(reflect::TypeDescriptor*, void*, string_view, Editor&);

using OnInspectGUICallback = bool(*)(void*, string_view, Editor&);

void register_on_inspect_gui(string_view, OnInspectGUICallback);
OnInspectGUICallback get_on_inspect_gui(string_view on_type);