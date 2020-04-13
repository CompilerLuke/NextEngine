#include "stdafx.h"
#include "custom_inspect.h"
#include "displayComponents.h"
#include <imgui/imgui.h>
#include "core/io/logger.h"
#include "assetTab.h"
#include <glm/gtc/type_ptr.hpp>
#include "graphics/renderer/ibl.h"
#include "graphics/renderer/renderer.h"
#include "grass.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "editor.h"

bool Quat_inspect(void* data, string_view prefix, Editor& editor) {
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

bool Vec3_inspect(void* data, string_view prefix, Editor& editor) {
	
	glm::vec3* ptr = (glm::vec3*)data;
	ImGui::PushID((long long)data);
	ImGui::InputFloat3(prefix.c_str(), glm::value_ptr(*ptr));
	ImGui::PopID();

	return true;
}

bool Vec2_inspect(void* data, string_view prefix, Editor& editor) {
	glm::vec2* ptr = (glm::vec2*)data;

	ImGui::PushID((long long)data);
	ImGui::InputFloat2(prefix.c_str(), glm::value_ptr(*ptr));
	ImGui::PopID();
	return true;
}

bool Mat4_inspect(void* data, string_view prefix, Editor& editor) {
	ImGui::LabelText("Matrix", prefix.c_str());
	return true;
}

//Shaders
bool Shader_inspect(void* data, string_view prefix, Editor& editor) { //todo how do we pass state into these functions
	Assets& assets = editor.asset_manager;
	auto handle_shader = (shader_handle*)data;
	auto shader = shader_info(assets, *handle_shader);
	if (!shader) return true;

	auto name = tformat(shader->vfilename, ", ", shader->ffilename);
	ImGui::LabelText(prefix.c_str(), name.c_str());

	return true;
}

template<typename H, typename T>
bool render_asset_preview(Assets& assets, H* handle_ptr, vector<T*>& to_asset, string_view prefix, const char * drag_and_drop) {
	H asset_id = *handle_ptr;

	T* asset = NULL;
	if (asset_id.id < to_asset.length) asset = to_asset[asset_id.id]; //todo probably want reference to editor

	if (asset == NULL) {
		ImGui::Text("Not set");
		ImGui::NewLine();
	}
	else {
		ImGui::Image(assets, asset->rot_preview.preview, ImVec2(128, 128));
	}

	accept_drop(drag_and_drop, handle_ptr, sizeof(uint));

	ImGui::SameLine();
	if (asset) ImGui::Text(tformat(prefix, " : ", asset->name).c_str());

	return true;
}

bool Model_inspect(void* data, string_view prefix, Editor& editor) {
	return render_asset_preview(editor.asset_manager, (model_asset_handle*)data, editor.asset_tab.model_handle_to_asset, prefix, "DRAG_AND_DROP_MODEL");
}

bool Layermask_inspect(void* data, string_view prefix, Editor& editor) {
	Layermask* mask_ptr = (Layermask*)(data);
	Layermask mask = *mask_ptr;

	if (ImGui::RadioButton("game", mask & GAME_LAYER)) {
		mask ^= GAME_LAYER;
	}



	ImGui::SameLine();

	if (ImGui::RadioButton("editor", mask & EDITOR_LAYER)) {
		mask ^= EDITOR_LAYER;
	}

	ImGui::SameLine();
	ImGui::Text("layermask");

	*mask_ptr = mask;
	return true;
}

bool EntityEditor_inspect(void* data, string_view prefix, Editor& world) {
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
bool Material_inspect(void* data, string_view prefix, Editor& editor) {
	return render_asset_preview(editor.asset_manager, (material_asset_handle*)data, editor.asset_tab.material_handle_to_asset, prefix, "DRAG_AND_DROP_MATERIAL");
}

bool Materials_inspect(void* data, string_view prefix, Editor& editor) {
	Materials* materials = (Materials*)data;

	if (ImGui::CollapsingHeader("Materials")) {
		for (unsigned int i = 0; i < materials->materials.length; i++) {
			Material_inspect(&materials->materials[i], tformat("Element ", i), editor);
		}
		return true;
	}

	return false;
}

bool Skybox_inspect(void* data, string_view prefix, Editor& editor) {
	if (ImGui::CollapsingHeader("Skylight")) {
		if (ImGui::Button("Capture")) {
			((Skybox*)data)->capture_scene = true;
			
			Renderer& renderer = editor.renderer;
			renderer.skybox_renderer->load((Skybox*)data);
		}
		return true;
	}

	return false;
}

bool Grass_inspect(void* data, string_view prefix, Editor& editor) {
	World& world = editor.world;
	Grass* grass = (Grass*)data;

	static glm::vec2 density_range(0, 0.1);
	
	if (ImGui::CollapsingHeader("Grass")) {
		Grass* grass = (Grass*)data;
		
		auto type = (reflect::TypeDescriptor_Struct*)reflect::TypeResolver<Grass>::get();
		for (auto& member : type->members) {
			if (member.tag == reflect::HideInInspectorTag) continue;
			if (member.name == "density") {
				ImGui::InputFloat2("density range", &density_range.x);
				ImGui::SliderFloat("density", &grass->density, density_range.x, density_range.y);
				continue;
			}

			render_fields(member.type, (char*)data + member.offset, member.name, editor);
		}

		ImGui::NewLine();

		if (ImGui::Button("Place")) {
			place_Grass(grass, world);
		}

		return true;
	}

	ID id = world.id_of<Grass>(grass);

	Model* model = get_Model(editor.asset_manager, grass->placement_model);
	Materials* materials = world.by_id<Materials>(id);
	if (materials) {
		int diff = model->materials.length - materials->materials.length;

		if (diff > 0) {
			for (int i = 0; i < diff; i++) {
				materials->materials.append({ INVALID_HANDLE });
			}
		}
		else {
			for (int i = 0; i < -diff; i++) {
				materials->materials.pop();
			}
		}
	}

	return false;
}

void register_on_inspect_callbacks() {
	register_on_inspect_gui("Skybox", Skybox_inspect);
	register_on_inspect_gui("Material", Material_inspect);
	register_on_inspect_gui("Materials", Materials_inspect);
	register_on_inspect_gui("shader_handle", Shader_inspect);
	register_on_inspect_gui("model_handle", Model_inspect);
	register_on_inspect_gui("Layermask", Layermask_inspect);
	register_on_inspect_gui("EntityEditor", EntityEditor_inspect);
	register_on_inspect_gui("Grass", Grass_inspect);
	
	register_on_inspect_gui("glm::vec2", Vec2_inspect);
	register_on_inspect_gui("glm::quat", Quat_inspect);
	register_on_inspect_gui("glm::vec3", Vec3_inspect);
}

