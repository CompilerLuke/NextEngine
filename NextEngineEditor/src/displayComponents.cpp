#include "stdafx.h"
#include "displayComponents.h"
#include <imgui/imgui.h>
#include "editor.h"
#include "ecs/system.h"
#include "ecs/ecs.h"
#include "core/io/logger.h"
#include "core/container/hash_map.h"

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

const char* destroy_component_popup_name(reflect::TypeDescriptor_Struct* type) {
	return tformat("DestroyComponent", type->name).c_str();
}

bool render_fields_struct(reflect::TypeDescriptor_Struct* self, void* data, string_view prefix, Editor& editor) {
	if (override_inspect.keys.index(self->name) != -1) {
		return override_inspect[self->name](data, prefix, editor);
	}
	
	auto name = tformat(prefix, " : ", self->name);

	auto id = ImGui::GetID(name.c_str());

	if (self->members.length == 1 && prefix != "Component") {
		auto& field = self->members[0];
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
		for (auto field : self->members) {
			auto offset_ptr = (char*)data + field.offset;
			
			if (field.tag == reflect::LayermaskTag) {
				override_inspect["Layermask"](offset_ptr, prefix, editor);
			}
			else if (field.tag == reflect::HideInInspectorTag) {}
			else {
				render_fields(field.type, offset_ptr, field.name, editor);
			}
		}
		if (prefix != "Component") ImGui::TreePop();
	}
	return open;
}

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
}

bool render_fields_enum(reflect::TypeDescriptor_Enum* self, void* data, string_view prefix, Editor& world) {
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

bool render_fields_ptr(reflect::TypeDescriptor_Pointer* self, void* data, string_view prefix, Editor& world) {
	if (*(void**)data == NULL) {
		ImGui::LabelText(prefix.c_str(), "NULL");
		return false;
	}
	return render_fields(self->itemType, *(void**)data, prefix, world);
}

bool render_fields_vector(reflect::TypeDescriptor_Vector* self, void* data, string_view prefix, Editor& world) {
	auto ptr = (vector<char>*)data;
	data = ptr->data;

	auto name = tformat(prefix, " : ", self->name);
	if (ImGui::TreeNode(name.c_str())) {
		for (unsigned int i = 0; i < ptr->length; i++) {
			auto name = "[" + std::to_string(i) + "]";
			render_fields(self->itemType, (char*)data + (i * self->itemType->size), name.c_str(), world);
		}
		ImGui::TreePop();
		return true;
	}
	return false;
}

bool render_fields(reflect::TypeDescriptor* type, void* data, string_view prefix, Editor& editor) {
	if (type->kind == reflect::Struct_Kind) return render_fields_struct((reflect::TypeDescriptor_Struct*)type, data, prefix, editor);
	else if (type->kind == reflect::Union_Kind) return render_fields_union((reflect::TypeDescriptor_Union*)type, data, prefix, editor);
	else if (type->kind == reflect::Vector_Kind) return render_fields_vector((reflect::TypeDescriptor_Vector*)type, data, prefix, editor);
	else if (type->kind == reflect::Enum_Kind) return render_fields_enum((reflect::TypeDescriptor_Enum*)type, data, prefix, editor);
	else if (type->kind == reflect::Float_Kind) return render_fields_primitive((float*)data, prefix);
	else if (type->kind == reflect::Int_Kind) return render_fields_primitive((int*)data, prefix);
	else if (type->kind == reflect::Bool_Kind) return render_fields_primitive((bool*)data, prefix);
	else if (type->kind == reflect::Unsigned_Int_Kind) return render_fields_primitive((unsigned int*)data, prefix);
	else if (type->kind == reflect::StringBuffer_Kind) return render_fields_primitive((string_buffer*)data, prefix);
}

void DisplayComponents::update(World& world, UpdateCtx& params) {}

void DisplayComponents::render(World& world, RenderCtx& params, Editor& editor) {
	//ImGui::SetNextWindowSize(ImVec2(params.width * editor.editor_tab_width, params.height));
	//ImGui::SetNextWindowPos(ImVec2(0, 0));

	if (ImGui::Begin("Properties", NULL)) {
		int selected_id = editor.selected_id;

		if (selected_id >= 0) {
			auto name_and_id = std::string("Entity #") + std::to_string(selected_id);

			auto meta = world.by_id<EntityEditor>(selected_id);
			if (meta) {
				char buff[50];
				memcpy(buff, meta->name.c_str(), meta->name.size() + 1);
				ImGui::InputText(name_and_id.c_str(), buff, 50);
				meta->name = buff;
			}
			else {
				ImGui::Text(name_and_id.c_str());
			}

			bool uncollapse = ImGui::Button("uncollapse all");

			auto components = world.components_by_id(selected_id);

			for (auto& comp : components) {
				ImGui::BeginGroup();
				if (uncollapse)
					ImGui::SetNextTreeNodeOpen(true);

				DiffUtil diff_util(comp.data, comp.type, &temporary_allocator);

				bool open = render_fields(comp.type, comp.data, "Component", editor);
				if (open) {
					ImGui::Dummy(ImVec2(10, 20));
				}

				if (ImGui::BeginPopup(destroy_component_popup_name((reflect::TypeDescriptor_Struct*)comp.type))) {
					if (ImGui::Button("Delete")) {
						for (int i = 0; i < world.components_hash_size; i++) {
							ComponentStore* store = world.components[i].get();
							if (store && string_view(store->get_component_type()->name) == comp.type->name) {
								editor.submit_action(new DestroyComponentAction(store, selected_id));

								break;
							}
						}

						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				} else {
					diff_util.submit(editor, "Edited Property");
				}

				ImGui::EndGroup();
			}

			if (ImGui::Button("Add Component")) {
				ImGui::OpenPopup("createComponent");
			}
			if (ImGui::BeginPopup("createComponent")) {
				char buff[50];
				memcpy(buff, filter.c_str(), filter.length + 1);
				ImGui::InputText("filter", buff, 50);
				filter = string_buffer(buff);

				for (int i = 0; i < world.components_hash_size; i++) {
					ComponentStore* store = world.components[i].get();
					if (store == NULL) continue;
					if (store->get_by_id(selected_id).data != NULL) continue;

					string_view type_name = string_view(store->get_component_type()->name);

					if (!type_name.starts_with_ignore_case(filter)) continue;

					if (ImGui::Button(type_name.c_str())) { //todo make work with undo and redo
						store->make_by_id(selected_id);
						ImGui::CloseCurrentPopup();
					}
				}
				ImGui::EndPopup();
			}
		}
	}

	ImGui::End();
}