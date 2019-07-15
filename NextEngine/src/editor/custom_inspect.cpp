#include "stdafx.h"
#include "editor/custom_inspect.h"
#include "editor/displayComponents.h"
#include "graphics/rhi.h"
#include "imgui.h"
#include "logger/logger.h"
#include "editor/assetTab.h"

//Shaders
bool Shader_inspect(void* data, struct reflect::TypeDescriptor* type, const std::string& prefix, struct World& world) {
	auto handle_shader = (Handle<Shader>*)data;
	auto shader = RHI::shader_manager.get(*handle_shader);

	auto name = shader->v_filename + std::string(", ") + shader->f_filename;
	ImGui::LabelText(prefix.c_str(), name.c_str());

	return true;
}

bool Model_inspect(void* data, reflect::TypeDescriptor* type, const std::string& prefix, World& world) {
	Handle<Model> model_id = *(Handle<Model>*)(data);
	if (model_id.id == INVALID_HANDLE) {
		ImGui::LabelText("name", "unselected");
		return true;
	}

	Model* model = RHI::model_manager.get(model_id);
	if (model) {
		auto quoted = "\"" + model->path + "\"";
		ImGui::LabelText("name", quoted.c_str());
	}
	else {
		ImGui::LabelText("name", "unselected");
	}
	return true;
}

bool Layermask_inspect(void* data, reflect::TypeDescriptor* type, const std::string& prefix, World& world) {
	Layermask* mask_ptr = (Layermask*)(data);
	Layermask mask = *mask_ptr;

	if (ImGui::RadioButton("game", mask & game_layer)) {
		mask ^= game_layer;
	}

	ImGui::SameLine();

	if (ImGui::RadioButton("editor", mask & editor_layer)) {
		mask ^= editor_layer;
	}

	ImGui::SameLine();
	ImGui::Text("layermask");

	*mask_ptr = mask;
	return true;
}

bool EntityEditor_inspect(void* data, reflect::TypeDescriptor* type, const std::string& prefix, World& world) {
	return false;
}

void accept_drop(const char* drop_type, void* ptr, unsigned int size) {
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(drop_type)) {
			memcpy(ptr, payload->Data, size);
		}
		ImGui::EndDragDropTarget();
	}
}

//Materials
bool Material_inspect(void* data, reflect::TypeDescriptor* type, const std::string& prefix, World& world) {
	Handle<Material>* handle_ptr = (Handle <Material>*)data;
	Handle<Material> material = *handle_ptr;

	MaterialAsset* material_asset = AssetTab::material_handle_to_asset[material.id];

	auto name = prefix + std::string(" ") + material_asset->name;

	ImGui::Image((ImTextureID)texture::id_of(material_asset->rot_preview.preview), ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));
	
	accept_drop("DRAG_AND_DROP_MATERIAL", handle_ptr, sizeof(Handle<Material>));
	
	ImGui::SameLine();
	ImGui::Text(name.c_str());

	return false;
}

bool Materials_inspect(void* data, reflect::TypeDescriptor* type, const std::string& prefix, World& world) {
	Materials* materials = (Materials*)data;
	auto material_type = (reflect::TypeDescriptor_Struct*)type;

	if (ImGui::CollapsingHeader("Materials")) {
		for (unsigned int i = 0; i < materials->materials.length; i++) {
			std::string prefix = "Element ";
			prefix += std::to_string(i);
			prefix += " :";

			Material_inspect(&materials->materials[i], reflect::TypeResolver<Material>::get(), prefix, world);
		}
		return true;
	}

	return false;
}

void register_on_inspect_callbacks() {
	register_on_inspect_gui("Material", Material_inspect);
	register_on_inspect_gui("Materials", Materials_inspect);
	register_on_inspect_gui("Handle<Shader>", Shader_inspect);
	register_on_inspect_gui("Handle<Model>", Model_inspect);
	register_on_inspect_gui("Layermask", Layermask_inspect);
	register_on_inspect_gui("EntityEditor", EntityEditor_inspect);
}

