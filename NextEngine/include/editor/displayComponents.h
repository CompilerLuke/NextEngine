#pragma once

#include "imgui.h"
#include "core/vector.h"
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <string>
#include <glm/mat4x4.hpp>
#include "reflection/reflection.h"

struct DisplayComponents {
	vector<float> fps_times;

	void update(struct World& world, struct UpdateParams& params);
	void render(struct World& world, struct RenderParams& params, struct Editor& editor);
};

bool render_fields_primitive(glm::quat*, const std::string&);
bool render_fields_primitive(glm::vec3*, const std::string&);
bool render_fields_primitive(glm::vec2*, const std::string&);
bool render_fields_primitive(glm::mat4*, const std::string&);
bool render_fields_primitive(int*, const std::string&);
bool render_fields_primitive(unsigned int*, const std::string&);
bool render_fields_primitive(float*, const std::string&);
bool render_fields_primitive(std::string*, const std::string&);
bool render_fields_primitive(bool*, const std::string&);

using OnInspectGUICallback = bool(*)(void*, reflect::TypeDescriptor*, const std::string&, World&);

void register_on_inspect_gui(const std::string&, OnInspectGUICallback);