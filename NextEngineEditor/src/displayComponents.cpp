#include "displayComponents.h"
#include <imgui/imgui.h>
#include "editor.h"
#include "ecs/system.h"
#include "ecs/ecs.h"
#include "core/io/logger.h"
#include "core/container/hash_map.h"
#include "core/container/string_view.h"
#include "graphics/assets/assets.h"


hash_map<sstring, OnInspectGUICallback, 103> override_inspect;

OnInspectGUICallback get_on_inspect_gui(string_view on_type) {
	return override_inspect[on_type];
}

void register_on_inspect_gui(string_view on_type, OnInspectGUICallback func) {
	override_inspect[on_type.data] = func;
}

bool render_fields_primitive(int* ptr, string_view prefix) {
	ImGui::PushID((long long)ptr);
	ImGui::InputInt(prefix.c_str(), ptr);
	ImGui::PopID();
	return true;
}

bool render_fields_primitive(unsigned int* ptr, string_view prefix) {
	int as_int = *ptr;
	ImGui::PushID((long long)ptr);

	ImGui::InputInt(prefix.c_str(), &as_int);
	ImGui::PopID();
	*ptr = as_int;
	return true;
}

bool render_fields_primitive(float* ptr, string_view prefix) {
	ImGui::PushID((long long)ptr);
	ImGui::InputFloat(prefix.c_str(), ptr);
	ImGui::PopID();
	return true;
}

bool render_fields_primitive(string_buffer* str, string_view prefix) {
	ImGui::PushID((long long)str);
	ImGui::InputText(prefix.c_str(), *str);
	ImGui::PopID();
	return true;
}

bool render_fields_primitive(bool* ptr, string_view prefix) {
	ImGui::PushID((long long)ptr);
	ImGui::Checkbox(prefix.c_str(), ptr);
	ImGui::PopID();
	return true;
}

#include <imgui/imgui_internal.h>

namespace ImGui {
	bool ImageButton(texture_handle handle, glm::vec2 size, glm::vec2 uv0, glm::vec2 uv1) {
		if (handle.id == INVALID_HANDLE) handle = default_textures.white;

		return ImageButton((ImTextureID)handle.id, size, uv0, uv1); 
	}

	void Image(texture_handle handle, glm::vec2 size, glm::vec2 uv0, glm::vec2 uv1) {
		if (handle.id == INVALID_HANDLE) handle = default_textures.white;

		Image( (ImTextureID)handle.id, size, uv0, uv1); 
	}
}

const char* destroy_component_popup_name(refl::Struct* type) {
	return tformat("DestroyComponent", type->name).c_str();
}

bool render_fields_struct(refl::Struct* self, void* data, string_view prefix, Editor& editor) {
	if (override_inspect.index(self->name) != -1) {
		return override_inspect[self->name](data, prefix, editor);
	}
	
	auto name = tformat(prefix, " : ", self->name);

	auto id = ImGui::GetID(name.c_str());

	if (self->fields.length == 1 && prefix != "Component") {
		auto& field = self->fields[0];
		render_fields(field.type, (char*)data + field.offset, field.name, editor);
		return true;
	}

	bool open;
	if (prefix == "Component") {
		open = ImGui::CollapsingHeader(self->name.c_str(), ImGuiTreeNodeFlags_Framed);
		if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[1]) {

			ImGui::OpenPopup(destroy_component_popup_name(self));
		}

		ImGui::PushID(self->name.c_str());
		//ImGui::CloseButton(ImGui::GetActiveID(), ImVec2(ImGui::GetCurrentWindow()->DC.CursorPos.x + ImGui::GetWindowWidth() - 35, ImGui::GetCurrentWindow()->DC.CursorPos.y - 23.0f), 15.0f);
		ImGui::PopID();
	}
	else {
		open = ImGui::TreeNode(name.c_str());
	}

	if (open) {
		for (auto field : self->fields) {
			auto offset_ptr = (char*)data + field.offset;
			
			if (field.flags == refl::HIDE_IN_EDITOR_TAG) {}
			else {
				render_fields(field.type, offset_ptr, field.name, editor);
			}
		}
		if (prefix != "Component") ImGui::TreePop();
	}
	return open;
}

/*
bool render_fields_union(reflect::TypeDescriptor_Union* self, void* data, string_view prefix, Editor& editor) {

	int tag = *((char*)data + self->tag_offset);

	auto name = tformat(prefix, " : ", self->name);

	if (ImGui::TreeNode(name.c_str())) {
		int i = 0;
		for (auto& member : self->cases) {
			if (i > 0) ImGui::SameLine();
			ImGui::RadioButton(member.name, tag == i);
			i++;
		}

		for (auto field : self->members) {
			render_fields(field.type, (char*)data + field.offset, field.name, editor);
		}

		auto& union_case = self->cases[tag];
		render_fields(union_case.type, (char*)data + union_case.offset, "", editor);

		ImGui::TreePop();
		return true;
	}
	return false;
}*/


bool render_fields_enum(refl::Enum* self, void* data, string_view prefix, Editor& world) {
	int tag = *((int*)data);

	int i = 0;
	for (auto& value : self->values) {
		if (i > 0) ImGui::SameLine();
		ImGui::RadioButton(value.name.c_str(), value.value == tag);
		i++;
	}

	ImGui::SameLine();
	ImGui::Text(prefix.c_str());

	return true;
}

string_view type_to_string(refl::Type* type) {
	switch (type->type) {
	case refl::Type::UInt: return "uint";
	case refl::Type::Int: return "int";
	case refl::Type::Bool: return "bool";
	case refl::Type::Float: return "float";
	}
}

bool render_fields_ptr(refl::Ptr* self, void* data, string_view prefix, Editor& world) {
	if (*(void**)data == NULL) {
		ImGui::LabelText(prefix.c_str(), "NULL");
		return false;
	}
	return render_fields(self->element, *(void**)data, prefix, world);
}

bool render_fields_vector(refl::Array* self, void* data, string_view prefix, Editor& world) {
	auto ptr = (vector<char>*)data;
	data = ptr->data;

	auto name = tformat(prefix, " : ", self->element->name);
	if (ImGui::TreeNode(name.c_str())) {
		for (unsigned int i = 0; i < ptr->length; i++) {
			auto name = "[" + std::to_string(i) + "]";
			render_fields(self->element, (char*)data + (i * self->element->size), name.c_str(), world);
		}
		ImGui::TreePop();
		return true;
	}
	return false;
}

bool render_fields(refl::Type* type, void* data, string_view prefix, Editor& editor) {
	if (override_inspect.index(type->name) != -1) return override_inspect[type->name](data, prefix, editor);

	if (type->type == refl::Type::Struct) return render_fields_struct((refl::Struct*)type, data, prefix, editor);
	//else if (type->kind == refl::Union_Kind) return render_fields_union((reflect::TypeDescriptor_Union*)type, data, prefix, editor);
	else if (type->type == refl::Type::Array) return render_fields_vector((refl::Array*)type, data, prefix, editor);
	else if (type->type == refl::Type::Enum) return render_fields_enum((refl::Enum*)type, data, prefix, editor);
	else if (type->type == refl::Type::Float) return render_fields_primitive((float*)data, prefix);
	else if (type->type == refl::Type::Int) return render_fields_primitive((int*)data, prefix);
	else if (type->type == refl::Type::Bool) return render_fields_primitive((bool*)data, prefix);
	else if (type->type == refl::Type::UInt) return render_fields_primitive((unsigned int*)data, prefix);
	else if (type->type == refl::Type::StringBuffer) return render_fields_primitive((string_buffer*)data, prefix);
}

void DisplayComponents::update(World& world, UpdateCtx& params) {}

//todo bug when custom ui for component, as it does not respect prefix!
void DisplayComponents::render(World& world, RenderPass& params, Editor& editor) {
	//ImGui::SetNextWindowSize(ImVec2(params.width * editor.editor_tab_width, params.height));
	//ImGui::SetNextWindowPos(ImVec2(0, 0));

	if (ImGui::Begin("Properties", NULL)) {
		int selected_id = editor.selected_id;

		if (selected_id >= 0) {
			auto name_and_id = tformat("Entity #", selected_id);

			EntityNode* node = editor.lister.by_id[selected_id];
			if (node) {
				ImGui::InputText(name_and_id.c_str(), node->name);
			}
			else {
				ImGui::Text(name_and_id.c_str());
			}

			Archetype arch = world.arch_of_id(selected_id);

			bool uncollapse = ImGui::Button("uncollapse all");

			for (uint component_id = 0; component_id < MAX_COMPONENTS; component_id++) {
				if (!has_component(arch, component_id)) continue;

				ComponentKind kind = world.component_kind[component_id];
				refl::Struct* type = world.component_type[component_id];
				if (kind != REGULAR_COMPONENT || !type) continue; //todo add editor support for component flags
				void* data = world.id_to_ptr[component_id][selected_id];

				ImGui::BeginGroup();
				
				if (uncollapse)
					ImGui::SetNextTreeNodeOpen(true);

				DiffUtil diff_util;
				begin_e_tdiff(diff_util, world, component_id, selected_id);

				bool open = render_fields(type, data, "Component", editor);
				if (open) {
					ImGui::Dummy(ImVec2(10, 20));
				}

				if (ImGui::BeginPopup(destroy_component_popup_name(type))) {
					if (ImGui::Button("Delete")) {
						entity_destroy_component_action(editor.actions, component_id, selected_id);
	
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				} else {
					end_diff(editor.actions, diff_util, "Edited Property");
				}

				ImGui::EndGroup();
			}



			if (ImGui::Button("Add Component")) {
				ImGui::OpenPopup("createComponent");
			}
			if (ImGui::BeginPopup("createComponent")) {
				ImGui::InputText("filter", filter);

				for (uint i = 0; i < MAX_COMPONENTS; i++) {
					refl::Struct* type = world.component_type[i];
					if (!type || (1ull << i) & arch) continue;
					if (!((string_view)type->name).starts_with_ignore_case(filter)) continue;

					if (ImGui::Button(type->name.c_str())) { 
						entity_create_component_action(editor.actions, i, selected_id);
						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::EndPopup();
			}
		}
	}

	ImGui::End();
}
