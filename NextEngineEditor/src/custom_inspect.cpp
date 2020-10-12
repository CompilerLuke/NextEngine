#include "custom_inspect.h"
#include "displayComponents.h"
#include <imgui/imgui.h>
#include "core/io/logger.h"
#include "assetTab.h"
#include <glm/gtc/type_ptr.hpp>
#include "graphics/renderer/ibl.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "grass.h"
#include "editor.h"
#include "components/lights.h"

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
	auto handle_shader = (shader_handle*)data;
	auto shader = shader_info(*handle_shader);
	if (!shader) return true;

	auto name = tformat(shader->vfilename, ", ", shader->ffilename);
	ImGui::LabelText(prefix.c_str(), name.c_str());

	return true;
}

bool render_asset_preview(AssetTab& asset_tab, AssetNode::Type type, uint* asset_handle, string_view prefix) {
	AssetNode* node = NULL;
	if (*asset_handle != INVALID_HANDLE && *asset_handle < MAX_ASSETS) node = asset_tab.info.asset_type_handle_to_node[type][*asset_handle]; //todo probably want reference to editor

	if (node == NULL) {
		ImGui::Text("Not set");
		ImGui::NewLine();
	}
	else {
		preview_image(asset_tab.preview_resources, node->model.rot_preview, ImVec2(128, 128));
	}

	accept_drop(drop_types[type], asset_handle, sizeof(uint));

	ImGui::SameLine();
	if (node) ImGui::Text(tformat(prefix, " : ", node->asset.name).c_str());

	return true;
}

bool Model_inspect(void* data, string_view prefix, Editor& editor) {
	return render_asset_preview(editor.asset_tab, AssetNode::Model, (uint*)data, prefix);
}

bool toggle_button(const char* name, bool enabled) {
	ImGui::SameLine();

	if (enabled) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	else ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));

	bool clicked = false;
	if (ImGui::Button(name)) clicked = true;
	
	ImGui::PopStyleColor();

	return clicked;
}

bool ID_inspect(void* data, string_view prefix, Editor& editor) {
	ID* id = (ID*)data;
	
	ImGui::Text("%s #%i", prefix.c_str(), *id);
	return true;
}

bool Entity_inspect(void* data, string_view prefix, Editor& editor) {
	Entity* e = (Entity*)data;

	if (!ImGui::CollapsingHeader("Entity")) return false;

	ID_inspect(&e->id, "", editor);

	Archetype arch = editor.world.arch_of_id(e->id);
	Archetype old_arch = arch;

	bool editor_only = arch & EDITOR_ONLY;
	if (ImGui::Checkbox("Editor only", &editor_only)) {
		arch ^= EDITOR_ONLY;
	}


	ImGui::NewLine();
	if (toggle_button("Dynamic", !(arch & STATIC))) arch &= ~STATIC;
	if (toggle_button("Static", arch & STATIC)) arch |= STATIC;

	//POTENTIALLY DANGEROUS SINCE IT INVALIDATES ALL THE POINTERS
	if (arch != old_arch) {
		editor.world.change_archetype(e->id, old_arch, arch, false);
	}

	return true;
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
	return render_asset_preview(editor.asset_tab, AssetNode::Material, (uint*)data, prefix);
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

bool SkyLight_inspect(void* data, string_view prefix, Editor& editor) {
	if (ImGui::CollapsingHeader("Skylight")) {
		if (ImGui::Button("Capture")) {
			((SkyLight*)data)->capture_scene = true;
			
			//todo implement skybox scene capture
		}
		return true;
	}

	return false;
}

bool Grass_inspect(void* data, string_view prefix, Editor& editor) {
	World& world = editor.world;
	Grass* grass = (Grass*)data;

	static glm::vec2 density_range(0, 0.1);

	ID id = editor.selected_id; //todo get correct ID!  // world.id_of<Grass>(grass);

	
	if (ImGui::CollapsingHeader("Grass")) {
		Grass* grass = (Grass*)data;
		

		//todo auto generate ui!
		Model_inspect(&grass->placement_model, "", editor);
		ImGui::Checkbox("cast_shadows", &grass->cast_shadows);
		ImGui::InputFloat2("covers", &grass->width);
		ImGui::InputFloat("max_height", &grass->max_height);
		ImGui::InputFloat2("density_range", &density_range.x);
		ImGui::SliderFloat("density", &grass->density, density_range.x, density_range.y);
		ImGui::SliderFloat("random_rotation", &grass->random_rotation, 0, 1);
		ImGui::Checkbox("align_to_terrain normal", &grass->align_to_terrain_normal);

		ImGui::NewLine();

		if (ImGui::Button("Place")) {
			place_Grass(world, id);
		}

		return true;
	}


	Model* model = get_Model(grass->placement_model);
	Materials* materials = world.m_by_id<Materials>(id);
	if (materials && model) {
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

#include "components/terrain.h"

void terrain_material_image(const char* name, texture_handle& handle) {
	ImGui::Text(name); 
	ImGui::SameLine(ImGui::GetContentRegionMax().x - 100);
	ImGui::Image(handle, ImVec2(128, 128));
	accept_drop("DRAG_AND_DROP_IMAGE", &handle, sizeof(texture_handle));
}

bool TerrainMaterials_inspect(void* data, string_view prefix, Editor& editor) {
	ImGui::Dummy(ImVec2(20, 10));
	
	if (ImGui::CollapsingHeader("Materials")) {
		vector<TerrainMaterial>& materials = *(vector<TerrainMaterial>*)data;

		for (TerrainMaterial& material : materials) {
			ImGui::InputText("name", material.name);

			terrain_material_image("diffuse", material.diffuse);
			terrain_material_image("metallic", material.metallic);
			terrain_material_image("roughness", material.roughness);
			terrain_material_image("normal", material.normal);
			terrain_material_image("height", material.height);
			terrain_material_image("ao", material.ao);
		}

		if (ImGui::Button("Add Material")) {
			TerrainMaterial material = {};
			material.name = "Material";
			material.diffuse = default_textures.white;
			material.metallic = default_textures.white;
			material.roughness = default_textures.white;
			material.normal = default_textures.normal;
			material.height = default_textures.white;
			material.ao = default_textures.white;

			materials.append(material);
		}
	}

	return true;
}

bool TerrainSplat_inspect(void* data, string_view prefix, Editor& editor) {
	if (ImGui::CollapsingHeader("TerrainSplat")) {
		TerrainSplat& splat = *(TerrainSplat*)data;

		auto[e, terrain] = *editor.world.first<Terrain>();

		ImGui::SliderFloat("hardness", &splat.hardness, 0.0f, 1.0f);
		ImGui::SliderFloat("min_height", &splat.min_height, 0.0f, terrain.max_height);
		ImGui::SliderFloat("max_height", &splat.max_height, 0.0f, terrain.max_height);

		ImGui::CollapsingHeader("Brush");
		ImGui::Image(load_Texture("editor/terrain/brush_low_res.png"), ImVec2(128, 128));
		
		ImGui::CollapsingHeader("Material");

		for (uint i = 0; i < terrain.materials.length; i++) {
			bool selected = i == splat.material;
			
			if (selected) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3);

			if (ImGui::ImageButton(terrain.materials[i].diffuse, ImVec2(128, 128))) {
				splat.material = i;
			}

			if (selected) ImGui::PopStyleVar();
		}
	}
}

void register_on_inspect_callbacks() {
	register_on_inspect_gui("Skybox", SkyLight_inspect);
	register_on_inspect_gui("Material", Material_inspect);
	register_on_inspect_gui("Materials", Materials_inspect);
	register_on_inspect_gui("shader_handle", Shader_inspect);
	register_on_inspect_gui("model_handle", Model_inspect);
	register_on_inspect_gui("Entity", Entity_inspect);
	register_on_inspect_gui("ID", ID_inspect);
	register_on_inspect_gui("Grass", Grass_inspect);
	register_on_inspect_gui("TerrainSplat", TerrainSplat_inspect);
	register_on_inspect_gui("vector<TerrainMaterial>", TerrainMaterials_inspect);

	
	register_on_inspect_gui("glm::vec2", Vec2_inspect);
	register_on_inspect_gui("glm::quat", Quat_inspect);
	register_on_inspect_gui("glm::vec3", Vec3_inspect);
}

