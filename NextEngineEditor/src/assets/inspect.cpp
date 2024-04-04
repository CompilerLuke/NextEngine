#include "graphics/assets/assets.h"
#include "graphics/assets/shader.h"
#include "graphics/rhi/pipeline.h"
#include "graphics/rhi/rhi.h"
#include <imgui/imgui.h>
#include "assets/node.h"
#include "assets/explorer.h"
#include "assets/inspect.h"
#include "custom_inspect.h"
#include "diffUtil.h"
#include "shaderGraph.h"

#include "../src/generated.h"
#include "editor.h"

struct AssetTab;
struct Editor;

bool edit_color(glm::vec4& color, string_view name, glm::vec2 size) {

	ImVec4 col(color.x, color.y, color.z, color.w);
	if (ImGui::ColorButton(name.c_str(), col, 0, ImVec2(size.x, size.y))) {
		ImGui::OpenPopup(name.c_str());
	}

	color = glm::vec4(col.x, col.y, col.z, col.w);

    bool active = false;
	if (ImGui::BeginPopup(name.c_str())) {
		ImGui::ColorPicker4(name.c_str(), &color.x);
        active = ImGui::IsItemActive();
		ImGui::EndPopup();
	}
    
    return active;
}

bool edit_color(glm::vec3& color, string_view name, glm::vec2 size) {
	glm::vec4 color4 = glm::vec4(color, 1.0f);
	bool active = edit_color(color4, name, size);
	color = color4;
    return active;
}

void channel_image(uint& image, string_view name, float scaling) {
    ImVec2 size(scaling * 200, scaling * 200);
    
    if (image == INVALID_HANDLE) { ImGui::Image(default_textures.white, size); }
	else ImGui::Image({ image }, size);

	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) {
        if (name == "normal") image = default_textures.normal.id;
        else image = INVALID_HANDLE;
	}

	accept_drop("DRAG_AND_DROP_IMAGE", &image, sizeof(texture_handle));
	ImGui::SameLine();

	ImGui::Text("%s", name.c_str());
	ImGui::SameLine(ImGui::GetWindowWidth() - 300 * scaling);
}

void texture_properties(TextureAsset* tex, Editor& editor) {
    TextureDesc& texture = *texture_desc(tex->handle);
    
    if (ImGui::CollapsingHeader("TextureInfo - Modified")) {
        ImGui::Text("Width : %i", texture.width);
        ImGui::Text("Height : %i", texture.height);
        ImGui::Text("Num Channels : %i", texture.num_channels);
        ImGui::Text("Mips : %i", texture.num_mips);
        
        TextureFormat format = texture.format;
        const char* format_str = "Unknown format";
        
        switch (texture.format) {
            case TextureFormat::U8: format_str = "U8"; break;
            case TextureFormat::UNORM: format_str = "UNORM"; break;
            case TextureFormat::SRGB: format_str = "SRGB"; break;
            case TextureFormat::HDR: format_str = "HDR"; break;
        }
        
        ImGui::Text("Format : %s", format_str);
    }
    
    ImGui::NewLine();
    ImGui::SetNextItemOpen(true);
    if (ImGui::CollapsingHeader("Preview")) {
        float size = 500 * editor.scaling;
        ImGui::Image(tex->handle, ImVec2(size, size));
    }
}

MaterialDesc base_shader_desc(shader_handle shader, bool tiling) {
	MaterialDesc desc{ shader };
	desc.draw_state = Cull_None;

	desc.mat_channel3("diffuse", glm::vec3(1.0f));
	desc.mat_channel1("metallic", 0.0f);
	desc.mat_channel1("roughness", 0.5f);
	desc.mat_channel1("normal", 1.0f, default_textures.normal);
	if (tiling) desc.mat_vec2("tiling", glm::vec2(5.0f));

	return desc;
}

#include "generated.h"

void begin_asset_diff(DiffUtil& util, AssetInfo& info, uint handle, AssetNode::Type type, void* copy) {
    refl::Type* types[AssetNode::Count] = {
        get_TextureAsset_type(),
        get_MaterialAsset_type(),
        get_ShaderAsset_type(),
        get_ModelAsset_type(),
        get_AssetFolder_type(),
    };
    
    ElementPtr ptr[2] = {};
    ptr[0].type = ElementPtr::AssetNode;
    ptr[0].id = handle;
    ptr[0].component_id = type;
    ptr[0].refl_type = types[type];
    ptr[1].type = ElementPtr::StaticPointer;
    ptr[1].ptr = &info;
    
    begin_diff(util, {ptr, 2}, copy);
}

void inspect_material_params(Editor& editor, material_handle handle, MaterialDesc* material) {
	static DiffUtil diff_util;
	static MaterialDesc copy;
	static material_handle last_asset = { INVALID_HANDLE };
    static bool slider_active;

    ElementPtr ptr[3] = {};
    ptr[0] = {ElementPtr::Offset, offsetof(MaterialAsset, desc), get_MaterialDesc_type()};
    ptr[1] = {ElementPtr::AssetNode, handle.id, AssetNode::Material, get_AssetNode_type()};
    ptr[2] = {ElementPtr::StaticPointer, &editor.asset_info};
    
	if (handle.id != last_asset.id || !slider_active) {
        begin_diff(diff_util, {ptr,3}, &copy);
        last_asset.id = handle.id;
	}
    
    slider_active = false;

    float scaling = editor.scaling;

	for (auto& param : material->params) {
		const char* name = param.name.data;

		if (param.type == Param_Vec2) {
			ImGui::PushItemWidth(200.0f);
			ImGui::InputFloat2(name, &param.vec2.x);
		}
		if (param.type == Param_Vec3) {
			edit_color(param.vec3, name);

			ImGui::SameLine();
			ImGui::Text(name);
		}
		if (param.type == Param_Image) {
			channel_image(param.image, param.name, scaling);
			ImGui::SameLine();
			ImGui::Text(name);
		}
		if (param.type == Param_Int) {
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::InputInt(name, &param.integer);
		}

		if (param.type == Param_Float) {
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::SliderFloat(name, &param.real, -1.0f, 1.0f);

			slider_active |= ImGui::IsItemActive();
		}

		if (param.type == Param_Channel3) {
			channel_image(param.image, name, scaling);
			slider_active |= edit_color(param.vec3, name, glm::vec2(50, 50));
		}

		if (param.type == Param_Channel2) {
			channel_image(param.image, name, scaling);
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::InputFloat2(tformat("##", name).c_str(), &param.vec2.x);
            slider_active |= ImGui::IsItemActive();
		}

		if (param.type == Param_Channel1) {
			channel_image(param.image, name, scaling);
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::SliderFloat(tformat("##", name).c_str(), &param.real, 0, 1.0f);
			slider_active |= ImGui::IsItemActive();
		}
	}

	if (slider_active) {
        commit_diff(editor.actions, diff_util);
	}
	else {
        end_diff(editor.actions, diff_util, "Material Property");
	}
}

void material_properties(MaterialAsset* mat_asset, Editor& editor, AssetTab& asset_tab) {
	AssetPreviewResources& previewer = asset_tab.preview_resources;
	
	auto& name = mat_asset->name;

	render_name(name, asset_tab.explorer.default_font);

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Material");

	MaterialDesc& desc = mat_asset->desc;
	if (false) { //todo check if handle becomes invalid, then again this should never happen 
		ImGui::Text("Material Handle is INVALID");
	}

	ShaderInfo* info = shader_info(desc.shader);
	if (info == NULL) {
		ImGui::Text("Shader Handle is INVALID!");
	}

	if (ImGui::Button(info->ffilename.data)) {
		ImGui::OpenPopup("StandardShaders");
	}


	if (accept_drop("DRAG_AND_DROP_SHADER", &desc.shader, sizeof(shader_handle))) {
		set_params_for_shader_graph(asset_tab, desc.shader);
	}


	if (ImGui::BeginPopup("StandardShaders")) {
		if (ImGui::Button("shaders/pbr.frag")) {
			shader_handle new_shad = default_shaders.pbr;
			if (new_shad.id != desc.shader.id) {
				desc = base_shader_desc(new_shad);
				//set_base_shader_params(assets, desc, new_shad);
				//material->set_vec2(shader_manager, "transformUVs", glm::vec2(1, 1));
			}
		}

		if (ImGui::Button("shaders/tree.frag")) {
			shader_handle new_shad = default_shaders.grass;
			if (new_shad.id != desc.shader.id) {
				//todo enable undo/redo for switching shaders

				desc = base_shader_desc(new_shad, false);
				desc.draw_state = Cull_None;
				desc.mat_float("cutoff", 0.5f);
			}
		}

		if (ImGui::Button("shaders/paralax_pbr.frag")) {
			shader_handle new_shad = load_Shader("shaders/pbr.vert", "shaders/paralax_pbr.frag");

			if (new_shad.id != desc.shader.id) {
				desc = base_shader_desc(new_shad);
				desc.mat_channel1("height", 1.0);
				desc.mat_channel1("ao", 1.0);
				desc.mat_int("steps", 5);
				desc.mat_float("depth_scale", 1);
				desc.mat_vec2("transformUVs", glm::vec2(1));
			}
		}

		ImGui::EndPopup();
	}

	inspect_material_params(editor, mat_asset->handle, &desc);

	rot_preview(previewer, mat_asset->rot_preview);
	render_preview_for(previewer, *mat_asset);
}

void model_properties(ModelAsset* mod_asset, Editor& editor, AssetTab& self) {
	Model* model = get_Model(mod_asset->handle);

	render_name(mod_asset->name, self.explorer.default_font);

	DiffUtil diff_util;
    ModelAsset copy;
    begin_asset_diff(diff_util, editor.asset_info, mod_asset->handle.id, AssetNode::Model, &copy);
    
	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Transform");
	ImGui::InputFloat3("position", &mod_asset->trans.position.x);
	get_on_inspect_gui("glm::quat")(&mod_asset->trans.rotation, "rotation", editor);
	ImGui::InputFloat3("scale", &mod_asset->trans.scale.x);

	end_diff(editor.actions, diff_util, "Properties Material");

	if (ImGui::Button("Apply")) {
		begin_gpu_upload();
		load_Model(mod_asset->handle, mod_asset->path, compute_model_matrix(mod_asset->trans));
		end_gpu_upload();

		mod_asset->rot_preview.rot_deg = glm::vec2();
		mod_asset->rot_preview.current = glm::vec2();
		mod_asset->rot_preview.previous = glm::vec2();
		mod_asset->rot_preview.rot = glm::quat();
	}

	if (ImGui::CollapsingHeader("LOD")) {
		float height = 200.0f;
		float padding = 10.0f;
		float cull_distance = model->lod_distance.last();
		float avail_width = ImGui::GetContentRegionAvail().x - padding * 4;

		float last_dist = 0;

		auto draw_list = ImGui::GetForegroundDrawList();

		glm::vec2 cursor_pos = glm::vec2(ImGui::GetCursorScreenPos()) + glm::vec2(padding);

		ImGui::SetCursorPos(glm::vec2(ImGui::GetCursorPos()) + glm::vec2(0, height + padding * 2));

		ImU32 colors[MAX_MESH_LOD] = {
			ImColor(245, 66, 66),
			ImColor(245, 144, 66),
			ImColor(245, 194, 66),
			ImColor(170, 245, 66),
			ImColor(66, 245, 111),
			ImColor(66, 245, 212),
			ImColor(66, 126, 245),
			ImColor(66, 69, 245),
		};

		//todo logic could be simplified
		static int dragging = -1;
		static glm::vec2 last_drag_delta;
		glm::vec2 drag_delta = glm::vec2(ImGui::GetMouseDragDelta()) - last_drag_delta;

		uint lod_count = model->lod_distance.length;

		if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			dragging = -1;
			last_drag_delta = glm::vec2();
			drag_delta = glm::vec2();
		}

		for (uint i = 0; i < lod_count; i++) {
			float dist = model->lod_distance[i];
			float offset = last_dist / cull_distance * avail_width;
			float width = avail_width * (dist - last_dist) / cull_distance;

			draw_list->AddRectFilled(cursor_pos + glm::vec2(offset, 0), cursor_pos + glm::vec2(offset + width, height), colors[i]);

			char buffer[100];
			snprintf(buffer, 100, "LOD %i - %.1fm", i, dist);

			draw_list->AddText(cursor_pos + glm::vec2(offset + padding, padding), ImColor(255, 255, 255), buffer);

			last_dist = dist;

			bool last = i + 1 == lod_count;
			bool is_dragged = i == dragging;
			if (!last && (is_dragged || ImGui::IsMouseHoveringRect(cursor_pos + glm::vec2(offset + width - padding, 0), cursor_pos + glm::vec2(offset + width + padding, height)))) {
				draw_list->AddRectFilled(cursor_pos + glm::vec2(offset + width - padding, 0), cursor_pos + glm::vec2(offset + width + padding, height), ImColor(255, 255, 255));

				model->lod_distance[i] += drag_delta.x * cull_distance / avail_width;
				dragging = i;
			}
		}

		float last_cull_distance = cull_distance;
		ImGui::InputFloat("Culling distance", &cull_distance);
		if (last_cull_distance != cull_distance) {
			for (uint i = 0; i < lod_count - 1; i++) {
				model->lod_distance[i] *= cull_distance / last_cull_distance;
			}

			model->lod_distance.last() = cull_distance;
		}

		last_drag_delta = ImGui::GetMouseDragDelta();

		mod_asset->lod_distance = (slice<float>)model->lod_distance;
	}

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Materials");

	if (model->materials.length != mod_asset->materials.length) {
		mod_asset->materials.resize(model->materials.length);
	}

	for (int i = 0; i < mod_asset->materials.length; i++) {
		string_buffer prefix = tformat(model->materials[i], " : ");

		get_on_inspect_gui("Material")(&mod_asset->materials[i], prefix, editor);
	}

	end_diff(editor.actions, diff_util, "Asset Properties");

	rot_preview(self.preview_resources, mod_asset->rot_preview);
	render_preview_for(self.preview_resources, *mod_asset);
}


void asset_properties(AssetNode& node, Editor& editor, AssetTab& tab) {
	if (node.type == AssetNode::Texture) texture_properties(&node.texture, editor);
	if (node.type == AssetNode::Shader) shader_graph_properties(node, editor, tab); //todo fix these signatures
	if (node.type == AssetNode::Model) model_properties(&node.model, editor, tab);
	if (node.type == AssetNode::Material) material_properties(&node.material, editor, tab);
}
