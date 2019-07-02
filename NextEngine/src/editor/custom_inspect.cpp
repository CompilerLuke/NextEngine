#include "stdafx.h"
#include "editor/custom_inspect.h"
#include "editor/displayComponents.h"
#include "graphics/rhi.h"
#include "imgui.h"
#include "logger/logger.h"

//Shaders
bool Shader_inspect(void* data, struct reflect::TypeDescriptor* type, const std::string& prefix, struct World& world) {
	auto handle_shader = (Handle<Shader>*)data;
	auto shader = RHI::shader_manager.get(*handle_shader);

	auto name = shader->v_filename + std::string(", ") + shader->f_filename;
	ImGui::LabelText(prefix.c_str(), name.c_str());

	return true;
}

//Materials
bool Material_inspect(void* data, reflect::TypeDescriptor* type, const std::string& prefix, World& world) {
	Material* material = (Material*)data;
	auto material_type = (reflect::TypeDescriptor_Struct*)type;

	auto name = prefix + std::string(" ") + material->name;

	if (ImGui::TreeNode(name.c_str())) {
		for (auto field : material_type->members) {
			if (field.name == "name");
			else if (field.name == "params") {
				//if (ImGui::TreeNode("params")) {
				for (auto& param : material->params) {
					auto shader = RHI::shader_manager.get(material->shader);
					auto& uniform = shader->uniforms[param.loc.id];

					if (param.type == Param_Vec2) {
						ImGui::InputFloat2(uniform.name.c_str(), &param.vec2.x);
					}
					if (param.type == Param_Vec3) {
						ImGui::ColorPicker3(uniform.name.c_str(), &param.vec3.x);
					}
					if (param.type == Param_Image) {
						Texture* tex = RHI::texture_manager.get(param.image);
						ImGui::Image((ImTextureID)tex->texture_id, ImVec2(200, 200));
						ImGui::SameLine();
						ImGui::Text(uniform.name.c_str());
					}
					if (param.type == Param_Int) {
						ImGui::InputInt(uniform.name.c_str(), &param.integer);
					}
				}
				//ImGui::TreePop();
				
			}
			else {
				field.type->render_fields((char*)data + field.offset, field.name, world);
			}
		}
		ImGui::TreePop();
		return true;
	}

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

void register_on_inspect_callbacks() {
	register_on_inspect_gui("Material", Material_inspect);
	register_on_inspect_gui("Materials", Materials_inspect);
	register_on_inspect_gui("Handle<Shader>", Shader_inspect);
	register_on_inspect_gui("Handle<Model>", Model_inspect);
	register_on_inspect_gui("Layermask", Layermask_inspect);
	register_on_inspect_gui("EntityEditor", EntityEditor_inspect);
}

