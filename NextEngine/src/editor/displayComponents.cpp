#include "stdafx.h"
#include "editor/displayComponents.h"
#include "imgui.h"
#include "editor/editor.h"
#include "ecs/system.h"
#include "ecs/ecs.h"
#include <tuple>
#include "graphics/rhi.h"
#include "logger/logger.h"

std::unordered_map <std::string, OnInspectGUICallback> override_inspect;

OnInspectGUICallback get_on_inspect_gui(StringView on_type) {
	return override_inspect[std::string(on_type.data)];
}

void register_on_inspect_gui(StringView on_type, OnInspectGUICallback func) {
	override_inspect[std::string(on_type.data)] = func;
}

bool render_fields_primitive(int* ptr, StringView prefix) {
	ImGui::InputInt(prefix.c_str(), ptr);
	return true;
}

bool render_fields_primitive(unsigned int* ptr, StringView prefix) {
	int as_int = *ptr;
	ImGui::InputInt(prefix.c_str(), &as_int);
	*ptr = as_int;
	return true;
}

bool render_fields_primitive(float* ptr, StringView prefix) {
	ImGui::InputFloat(prefix.c_str(), ptr);
	return true;
}

bool render_fields_primitive(std::string* str, StringView prefix) {
	char buf[50];
	std::memcpy(buf, str->c_str(), str->size() + 1);

	ImGui::InputText(prefix.c_str(), buf, 50);
	*str = std::string(buf);
	return true;
}

bool render_fields_primitive(bool* ptr, StringView prefix) {
	ImGui::Checkbox(prefix.c_str(), ptr);
	return true;
}

bool render_fields_struct(reflect::TypeDescriptor_Struct* self, void* data, StringView prefix, World& world) {
	if (override_inspect.find(self->name) != override_inspect.end()) {
		return override_inspect[self->name](data, prefix, world);
	}
	
	auto name = tformat(prefix, " : ", self->name);

	auto id = ImGui::GetID(name.c_str());

	if (self->members.size() == 1) {
		auto& field = self->members[0];
		render_fields(field.type, (char*)data + field.offset, field.name, world);
		return true;
	}

	bool open;
	if (prefix == "Component") {
		open = ImGui::CollapsingHeader(self->name, ImGuiTreeNodeFlags_Framed);
	}
	else {
		open = ImGui::TreeNode(name.c_str());
	}

	if (open) {
		for (auto field : self->members) {
			auto offset_ptr = (char*)data + field.offset;
			
			if (field.tag == reflect::LayermaskTag) {
				override_inspect["Layermask"](data, prefix, world);
			}
			else if (field.tag == reflect::HideInInspectorTag) {}
			else {
				render_fields(field.type, offset_ptr, field.name, world);
			}
		}
		if (prefix != "Component") ImGui::TreePop();
	}
	return open;
}

bool render_fields_union(reflect::TypeDescriptor_Union* self, void* data, StringView prefix, struct World& world) {

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
			render_fields(field.type, (char*)data + field.offset, field.name, world);
		}

		auto& union_case = self->cases[tag];
		render_fields(union_case.type, (char*)data + union_case.offset, "", world);

		ImGui::TreePop();
		return true;
	}
	return false;
}

bool render_fields_enum(reflect::TypeDescriptor_Enum* self, void* data, StringView prefix, struct World& world) {
	int tag = *((int*)data);

	int i = 0;
	for (auto& value : self->values) {
		if (i > 0) ImGui::SameLine();
		ImGui::RadioButton(value.first.c_str(), value.second == tag);
		i++;
	}

	ImGui::SameLine();
	ImGui::Text(prefix.c_str());

	return true;
}

bool render_fields_ptr(reflect::TypeDescriptor_Pointer* self, void* data, StringView prefix, struct World& world) {
	if (*(void**)data == NULL) {
		ImGui::LabelText(prefix.c_str(), "NULL");
		return false;
	}
	return render_fields(self->itemType, *(void**)data, prefix, world);
}

bool render_fields_vector(reflect::TypeDescriptor_Vector* self, void* data, StringView prefix, struct World& world) {
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

bool render_fields(reflect::TypeDescriptor* type, void* data, StringView prefix, World& world) {
	if (type->kind == reflect::Struct_Kind) return render_fields_struct((reflect::TypeDescriptor_Struct*)type, data, prefix, world);
	else if (type->kind == reflect::Union_Kind) return render_fields_union((reflect::TypeDescriptor_Union*)type, data, prefix, world);
	else if (type->kind == reflect::Vector_Kind) return render_fields_vector((reflect::TypeDescriptor_Vector*)type, data, prefix, world);
	else if (type->kind == reflect::Enum_Kind) return render_fields_enum((reflect::TypeDescriptor_Enum*)type, data, prefix, world);
	else if (type->kind == reflect::Float_Kind) return render_fields_primitive((float*)data, prefix);
	else if (type->kind == reflect::Int_Kind) return render_fields_primitive((int*)data, prefix);
	else if (type->kind == reflect::Bool_Kind) return render_fields_primitive((bool*)data, prefix);
	else if (type->kind == reflect::Unsigned_Int_Kind) return render_fields_primitive((unsigned int*)data, prefix);
	else if (type->kind == reflect::StringBuffer_Kind) return render_fields_primitive((std::string*)data, prefix);
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

				bool open = render_fields(comp.type, comp.data, "Component", world);
				if (open) {
					ImGui::Dummy(ImVec2(10, 20));
				}

				diff_util.submit(editor, "Edited Property");

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