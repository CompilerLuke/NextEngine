#include "stdafx.h"
#include "editor/custom_inspect.h"
#include "editor/displayComponents.h"
#include "graphics/rhi.h"
#include "imgui.h"
#include "logger/logger.h"
#include "editor/assetTab.h"
#include <glm/gtc/type_ptr.hpp>
#include "graphics/ibl.h"
#include "editor/grass.h"

bool Quat_inspect(void* data, StringView prefix, struct World& world) {
	ImGui::PushID((long long)data);

	glm::quat* ptr = (glm::quat*)data;
	glm::vec3 euler = glm::eulerAngles(*ptr);
	glm::vec3 previous = euler;
	euler = glm::degrees(euler);

	ImGui::InputFloat3(prefix.c_str(), glm::value_ptr(euler));
	euler = glm::radians(euler);
	if (glm::abs(glm::length(previous - euler)) > 0.001)
		*ptr = glm::quat(euler);

	ImGui::PopID();
	return true;
}

bool Vec3_inspect(void* data, StringView prefix, struct World& world) {
	
	glm::vec3* ptr = (glm::vec3*)data;
	ImGui::PushID((long long)data);
	ImGui::InputFloat3(prefix.c_str(), glm::value_ptr(*ptr));
	ImGui::PopID();

	return true;
}

bool Vec2_inspect(void* data, StringView prefix, struct World& world) {
	glm::vec2* ptr = (glm::vec2*)data;

	ImGui::PushID((long long)data);
	ImGui::InputFloat2(prefix.c_str(), glm::value_ptr(*ptr));
	ImGui::PopID();
	return true;
}

bool Mat4_inspect(void* data, StringView prefix, struct World& world) {
	ImGui::LabelText("Matrix", prefix.c_str());
	return true;
}

//Shaders
bool Shader_inspect(void* data, StringView prefix, struct World& world) {
	auto handle_shader = (Handle<Shader>*)data;
	auto shader = RHI::shader_manager.get(*handle_shader);

	auto name = format(shader->v_filename, ", ", shader->f_filename);
	ImGui::LabelText(prefix.c_str(), name.c_str());

	return true;
}

template<typename T>
bool render_asset_preview(Handle<T>* handle_ptr, vector<T*>& to_asset, StringView prefix, const char * drag_and_drop) {
	Handle<T> asset_id = *handle_ptr;

	T* asset = NULL;
	if (asset_id.id < to_asset.length) asset = to_asset[asset_id.id]; //todo probably want reference to editor

	if (asset == NULL) {
		ImGui::Text("Not set");
		ImGui::NewLine();
	}
	else {
		ImGui::Image((ImTextureID)texture::id_of(asset->rot_preview.preview), ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));
	}

	accept_drop(drag_and_drop, handle_ptr, sizeof(Handle<T>));

	ImGui::SameLine();
	if (asset) ImGui::Text(tformat(prefix, " : ", asset->name).c_str());

	return true;
}

bool Model_inspect(void* data, StringView prefix, World& world) {
	return render_asset_preview((Handle<ModelAsset>*)data, AssetTab::model_handle_to_asset, prefix, "DRAG_AND_DROP_MODEL");
}

bool Layermask_inspect(void* data, StringView prefix, World& world) {
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

bool EntityEditor_inspect(void* data, StringView prefix, World& world) {
	return false;
}

bool accept_drop(const char* drop_type, void* ptr, unsigned int size) {
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(drop_type)) {
			memcpy(ptr, payload->Data, size);
			return true;
		}
		ImGui::EndDragDropTarget();
	}

	return false;
}

//Materials
bool Material_inspect(void* data, StringView prefix, World& world) {
	return render_asset_preview((Handle<MaterialAsset>*)data, AssetTab::material_handle_to_asset, prefix, "DRAG_AND_DROP_MATERIAL");
}

bool Materials_inspect(void* data, StringView prefix, World& world) {
	Materials* materials = (Materials*)data;
	auto material_type = reflect::TypeResolver<Material>::get();

	if (ImGui::CollapsingHeader("Materials")) {
		for (unsigned int i = 0; i < materials->materials.length; i++) {
			Material_inspect(&materials->materials[i], tformat("Element ", i), world);
		}
		return true;
	}

	return false;
}

bool Skybox_inspect(void* data, StringView prefix, World& world) {
	if (ImGui::CollapsingHeader("Skylight")) {
		if (ImGui::Button("Capture")) {
			((Skybox*)data)->capture_scene = true;
			((Skybox*)data)->on_load(world, true); //todo broken
		}
		return true;
	}

	return false;
}

bool Grass_inspect(void* data, StringView prefix, World& world) {
	if (ImGui::CollapsingHeader("Grass")) {
		auto type = (reflect::TypeDescriptor_Struct*)reflect::TypeResolver<Grass>::get();
		for (auto& member : type->members) {
			if (member.tag == reflect::HideInInspectorTag) continue;
			render_fields(member.type, (char*)data + member.offset, member.name, world);
		}

		ImGui::NewLine();

		if (ImGui::Button("Place")) {
			((Grass*)data)->place(world);
		}

		return true;
	}

	return false;
}

void register_on_inspect_callbacks() {
	register_on_inspect_gui("Skybox", Skybox_inspect);
	register_on_inspect_gui("Material", Material_inspect);
	register_on_inspect_gui("Materials", Materials_inspect);
	register_on_inspect_gui("Handle<Shader>", Shader_inspect);
	register_on_inspect_gui("Handle<Model>", Model_inspect);
	register_on_inspect_gui("Layermask", Layermask_inspect);
	register_on_inspect_gui("EntityEditor", EntityEditor_inspect);
	register_on_inspect_gui("Grass", Grass_inspect);
	
	register_on_inspect_gui("glm::vec2", Vec2_inspect);
	register_on_inspect_gui("glm::quat", Quat_inspect);
	register_on_inspect_gui("glm::vec3", Vec3_inspect);
}

