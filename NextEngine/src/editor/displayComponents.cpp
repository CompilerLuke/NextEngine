#include "stdafx.h"
#include "editor/displayComponents.h"
#include "imgui.h"
#include "editor/editor.h"
#include "ecs/system.h"
#include "ecs/ecs.h"
#include <glm/gtc/type_ptr.hpp>
#include <tuple>
#include "graphics/rhi.h"

std::unordered_map <std::string, OnInspectGUICallback> override_inspect;

void register_on_inspect_gui(const std::string& on_type, OnInspectGUICallback func) {
	override_inspect[on_type] = func;
}

bool render_fields_primitive(glm::quat* ptr, const std::string& prefix) {
	glm::vec3 euler = glm::eulerAngles(*ptr);
	euler = glm::degrees(euler);

	ImGui::InputFloat3(prefix.c_str(), glm::value_ptr(euler));
	euler = glm::radians(euler);

	*ptr = glm::quat(euler);
	return true;
}

bool render_fields_primitive(glm::vec3* ptr, const std::string& prefix) {
	ImGui::InputFloat3(prefix.c_str(), glm::value_ptr(*ptr));
	return true;
}

bool render_fields_primitive(glm::vec2* ptr, const std::string& prefix) {
	ImGui::InputFloat2(prefix.c_str(), glm::value_ptr(*ptr));
	return true;
}

bool render_fields_primitive(glm::mat4* ptr, const std::string& prefix) {
	ImGui::LabelText("Matrix", prefix.c_str());
	return true;
}

bool render_fields_primitive(int* ptr, const std::string& prefix) {
	ImGui::InputInt(prefix.c_str(), ptr);
	return true;
}

bool render_fields_primitive(unsigned int* ptr, const std::string& prefix) {
	int as_int = *ptr;
	ImGui::InputInt(prefix.c_str(), &as_int);
	*ptr = as_int;
	return true;
}

bool render_fields_primitive(float* ptr, const std::string& prefix) {
	ImGui::InputFloat(prefix.c_str(), ptr);
	return true;
}

bool render_fields_primitive(std::string* str, const std::string& prefix) {
	char buf[50];
	std::memcpy(buf, str->c_str(), str->size() + 1);

	ImGui::InputText(prefix.c_str(), buf, 50);
	*str = std::string(buf);
	return true;
}

bool render_fields_primitive(bool* ptr, const std::string& prefix) {
	ImGui::Checkbox(prefix.c_str(), ptr);
	return true;
}

bool reflect::TypeDescriptor::render_fields(void* data, const std::string& prefix, World& world) {
	return false;
}

bool reflect::TypeDescriptor_Struct::render_fields(void* data, const std::string& prefix, World& world) {
	if (override_inspect.find(this->getFullName()) != override_inspect.end()) {
		return override_inspect[this->getFullName()](data, this, prefix, world);
	}
	
	auto name = prefix + " : " + this->getFullName();

	auto id = ImGui::GetID(name.c_str());

	if (this->members.size() == 1) {
		auto& field = members[0];
		field.type->render_fields((char*)data + field.offset, field.name, world);
		return true;
	}

	bool open;
	if (prefix == "Component") {
		open = ImGui::CollapsingHeader(this->getFullName().c_str(), ImGuiTreeNodeFlags_Framed);
	}
	else {
		open = ImGui::TreeNode(name.c_str());
	}

	if (open) {
		for (auto field : this->members) {
			auto offset_ptr = (char*)data + field.offset;
			
			if (field.tag == LayermaskTag) {
				override_inspect["Layermask"](data, this, prefix, world);
			}
			else {
				field.type->render_fields(offset_ptr, field.name, world);
			}
		}
		if (prefix != "Component") ImGui::TreePop();
	}
	return open;
}

bool reflect::TypeDescriptor_Union::render_fields(void* data, const std::string& prefix, struct World& world) {

	int tag = *((char*)data + this->tag_offset);

	auto name = prefix + " : " + this->getFullName();

	if (ImGui::TreeNode(name.c_str())) {
		int i = 0;
		for (auto& member : cases) {
			if (i > 0) ImGui::SameLine();
			ImGui::RadioButton(member.name, tag == i);
			i++;
		}

		for (auto field : members) {
			field.type->render_fields((char*)data + field.offset, field.name, world);
		}

		auto& union_case = cases[tag];
		union_case.type->render_fields((char*)data + union_case.offset, "", world);

		ImGui::TreePop();
		return true;
	}
	return false;
}

bool reflect::TypeDescriptor_Enum::render_fields(void* data, const std::string& prefix, struct World& world) {
	int tag = *((int*)data);

	int i = 0;
	for (auto& value : values) {
		if (i > 0) ImGui::SameLine();
		ImGui::RadioButton(value.first.c_str(), value.second == tag);
		i++;
	}

	ImGui::SameLine();
	ImGui::Text(prefix.c_str());

	return true;
}

bool reflect::TypeDescriptor_Pointer::render_fields(void* data, const std::string& prefix, struct World& world) {
	if (*(void**)data == NULL) {
		ImGui::LabelText(prefix.c_str(), "NULL");
		return false;
	}
	return this->itemType->render_fields(*(void**)data, prefix, world);
}

bool reflect::TypeDescriptor_Vector::render_fields(void* data, const std::string& prefix, struct World& world) {
	auto ptr = (vector<char>*)data;
	data = ptr->data;

	auto name = prefix + " : " + this->getFullName();
	if (ImGui::TreeNode(name.c_str())) {
		for (unsigned int i = 0; i < ptr->length; i++) {
			auto name = "[" + std::to_string(i) + "]";
			this->itemType->render_fields((char*)data + (i * this->itemType->size), name.c_str(), world);
		}
		ImGui::TreePop();
		return true;
	}
	return false;
}

void DisplayComponents::update(World& world, UpdateParams& params) {
	if (fps_times.length >= 60)
		fps_times.shift(1);

	fps_times.append((int)1.0 / params.delta_time);
}

void DisplayComponents::render(World& world, RenderParams& params, Editor& editor) {
	ImGui::SetNextWindowSize(ImVec2(params.width * editor.editor_tab_width, params.height));
	//ImGui::SetNextWindowPos(ImVec2(0, 0));

	if (ImGui::Begin("Properties", NULL)) {

		if (editor.selected_id >= 0) {
			auto name_and_id = std::string("Entity #") + std::to_string(editor.selected_id);

			auto meta = world.by_id<EntityEditor>(editor.selected_id);
			if (meta) {
				char buff[50];
				memcpy(buff, meta->name.c_str(), meta->name.size() + 1);
				ImGui::InputText(name_and_id.c_str(), buff, 50);
				meta->name = std::string(buff);
			}
			else {
				ImGui::Text(name_and_id.c_str());
			}

			bool uncollapse = ImGui::Button("uncollapse all");

			auto components = world.components_by_id(editor.selected_id);

			for (auto& comp : components) {
				ImGui::BeginGroup();
				if (uncollapse)
					ImGui::SetNextTreeNodeOpen(true);

				bool open = comp.type->render_fields(comp.data, "Component", world);
				if (open) {
					ImGui::Dummy(ImVec2(10, 20));
				}

				ImGui::EndGroup();
			}

		}
	}

	ImGui::End();

	ImGui::SetNextWindowSize(ImVec2(300, 300));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	if (ImGui::Begin("Profiler", NULL)) {
		ImGui::PlotLines("FPS", fps_times.data, fps_times.length, 0, NULL, 0, 70, ImVec2(300, 240));
	}

	ImGui::PopStyleVar();
	ImGui::End();

}