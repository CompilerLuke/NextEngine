#include "engine/engine.h"
#include "assets/explorer.h"
#include "assets/dialog.h"
#include "assets/importer.h"
#include "assets/info.h"
#include "editor.h"
#include "core/io/logger.h"
#include <thread>
#include "engine/vfs.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "graphics/assets/material.h"
#include "graphics/rhi/draw.h"
#include "components/transform.h"
#include "components/camera.h"
#include <glm/glm.hpp>
#include "graphics/rhi/primitives.h"
#include "graphics/assets/material.h"
#include "custom_inspect.h"
#include "engine/input.h"
#include "components/lights.h"
#include "core/serializer.h"
#include <thread>
#include <mutex>
#include "diffUtil.h"
#include "graphics/renderer/renderer.h"
#include <imgui/imgui.h>
#include "core/types.h"

#include "graphics/rhi/draw.h"
#include "graphics/rhi/pipeline.h"

#include "core/time.h"

#include "assets/inspect.h"

void render_name(sstring& name, ImFont* font) {
	ImGui::SetNextItemWidth(128);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
	ImGui::PushItemWidth(-1);
	ImGui::PushID((void*)&name);
	if (font) ImGui::PushFont(font);
	ImGui::InputText("##", name);
	ImGui::PopItemWidth();
	if (font) ImGui::PopFont();
	ImGui::PopID();
	ImGui::PopStyleColor();
}

wchar_t* to_wide_char(const char* orig);

bool filtered(AssetExplorer& self, string_view name) {
	return !name.starts_with_ignore_case(self.filter);
}

const float ASSET_ICON_SIZE = 128.0f;

void render_asset(AssetExplorer& tab, AssetNode& node, texture_handle tex_handle, bool* deselect, float scaling, glm::vec2 uv0 = glm::vec2(0,0), glm::vec2 uv1 = glm::vec2(1,1)) {
	if (filtered(tab, node.asset.name)) return;
	
	bool selected = node.handle.id == tab.selected.id;
	if (selected) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3);

    ImVec2 size(ASSET_ICON_SIZE * scaling, ASSET_ICON_SIZE * scaling);
    
	ImGui::PushID(node.handle.id);
	bool is_selected = ImGui::ImageButton(tex_handle, size, uv0, uv1);
	if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[1]) { //Right click
		tab.selected = node.handle;
		*deselect = false;
		ImGui::PopID();
		ImGui::OpenPopup("EditAsset");
		ImGui::PushID(node.handle.id);
	}

	if (selected) ImGui::PopStyleVar(1);


	if (ImGui::BeginDragDropSource()) {

		ImGui::Image(tex_handle, size);
		ImGui::SetDragDropPayload(drop_types[node.type], &node.asset.handle, sizeof(uint));
		ImGui::EndDragDropSource();
	}
	else if (is_selected) {
		//*deselect = false;
		tab.selected = node.handle;
	}

	render_name(node.asset.name, tab.filename_font);

	ImGui::NextColumn();
	ImGui::PopID();
}


void render_asset(AssetExplorer& tab, AssetNode& node, RotatablePreview& preview, bool* deselect, float scaling) {
	texture_handle handle;
	glm::vec2 uv0, uv1;

	preview_from_atlas(tab.preview_resources, preview, &handle, &uv0, &uv1);
	render_asset(tab, node, tab.preview_resources.preview_atlas, deselect, scaling, uv0, uv1);
}

struct MoveToFolder {
	AssetNode* asset = NULL;
	asset_handle dst_folder = { INVALID_HANDLE };
	asset_handle src_folder = { INVALID_HANDLE };
};

void folder_move_drop_target(AssetInfo& tab, MoveToFolder& move_to_folder, asset_handle src_folder, asset_handle dst_folder) {
	if (ImGui::BeginDragDropTarget()) {
		accept_move_to_folder(tab, &move_to_folder.asset);
		if (move_to_folder.asset != nullptr) {
			move_to_folder.src_folder = src_folder;
			move_to_folder.dst_folder = dst_folder;
		}
		ImGui::EndDragDropTarget();
	}
}

void move_dropped_to_folder(AssetInfo& tab, MoveToFolder& move) {
	if (move.dst_folder.id != INVALID_HANDLE) move_to_folder(tab, move.src_folder, move.dst_folder, std::move(*move.asset));
}

void render_assets(Editor& editor, AssetExplorer& asset_tab, string_view filter, bool* deselect) {
	asset_handle folder_handle = asset_tab.current_folder;
	AssetFolder& folder = get_asset(asset_tab.info, folder_handle)->folder;
	
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16, 0.16, 0.16, 1));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16, 0.16, 0.16, 1));

    float scaling = editor.scaling;
	ImGui::Columns(max(1,ImGui::GetContentRegionAvailWidth() / (ASSET_ICON_SIZE + 10) * scaling), "", false);

	MoveToFolder move_to_folder = {};
    

	for (AssetNode& node : folder.contents) {
		if (node.type == AssetNode::Texture) render_asset(asset_tab, node, node.texture.handle, deselect, scaling);
		if (node.type == AssetNode::Shader) render_asset(asset_tab, node, editor.get_icon("shader"), deselect, scaling);
		if (node.type == AssetNode::Model) render_asset(asset_tab, node, node.model.rot_preview, deselect, scaling);
		if (node.type == AssetNode::Material) render_asset(asset_tab, node, node.material.rot_preview, deselect, scaling);

		if (node.type == AssetNode::Folder && !filtered(asset_tab, node.folder.name)) {
			ImGui::PushID(node.handle.id);
			if (ImGui::ImageButton(editor.get_icon("folder"), ImVec2(128 * scaling, 128 * scaling))) {
				asset_tab.current_folder = node.handle;
				*deselect = false;
			}
			ImGui::PopID();

			if (ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("DRAG_AND_DROP_FOLDER", &node.handle.id, sizeof(uint));
				ImGui::Image(editor.get_icon("folder"), ImVec2(128 * scaling, 128 * scaling));
				ImGui::EndDragDropSource();
			}

			folder_move_drop_target(asset_tab.info, move_to_folder, asset_tab.current_folder, node.handle);

			render_name(node.folder.name, asset_tab.filename_font);
			ImGui::NextColumn();
		}
	}

	ImGui::Columns(1);
	ImGui::PopStyleColor(2);

	move_dropped_to_folder(asset_tab.info, move_to_folder);
}


//Materials



void set_params_for_shader_graph(AssetTab& asset_tab, shader_handle shader);

AssetTab::AssetTab(Renderer& renderer, AssetInfo& info, Window& window) : 
	info(info),
	window(window), 
	renderer(renderer), 
	explorer{ info, preview_resources }
{
	make_AssetPreviewRenderData(preview_resources, renderer);	
	make_AssetInfo(info); //note: this is overwritten when loading a scene
	explorer.current_folder = info.toplevel.handle;
}

#include "graphics/rhi/window.h"

void AssetTab::register_callbacks(Window& window, Editor& editor) {
    drop_file_callback = window.on_drop.listen([this, &editor](const string_buffer& filename) {
        import_filename(editor, editor.world, *this, filename);
    }).id;
}

void AssetTab::remove_callbacks(Window& window, Editor& editor) {
    window.on_drop.remove({drop_file_callback});
}

void AssetTab::render(World& world, Editor& editor, RenderPass& ctx) {
	render_previews(preview_resources, info);
	
	if (ImGui::Begin("Assets")) {
		bool open_create_asset = false;
		bool deselect = false;
		
		if (ImGui::IsWindowHovered()) {
			//if (ImGui::GetIO().MouseClicked[0] && ImGui::IsAnyItemActive()) selected = -1;
			if (ImGui::GetIO().MouseClicked[1]) {
				deselect = true;
				open_create_asset = true;
			}
			if (ImGui::GetIO().MouseClicked[0]) {
				deselect = true;
			}
		}

		{
			asset_handle folder_tree = explorer.current_folder;
			tvector<string_view> folder_file_path;
			tvector<asset_handle> folder_id;

			while (folder_tree.id != INVALID_HANDLE) {
				folder_id.append(folder_tree);
				AssetFolder& folder = get_asset(info, folder_tree)->folder;
				folder_file_path.append(folder.name);
								
				folder_tree = folder.owner;
			}

			MoveToFolder move_to_folder;

			ImVec4 window_bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
			ImGui::PushStyleColor(ImGuiCol_Button, window_bg);

			for (int i = folder_file_path.length - 1; i >= 0; i--) {
				if (ImGui::Button(folder_file_path[i].c_str())) {
					explorer.current_folder = folder_id[i];
				}

				folder_move_drop_target(info, move_to_folder, explorer.current_folder, folder_id[i]);

				ImGui::SameLine();
				ImGui::Text("\\");
				ImGui::SameLine();
			}

			ImGui::PopStyleColor();
			move_dropped_to_folder(info, move_to_folder);
		}

		{
			ImGui::SameLine(ImGui::GetWindowWidth() - 400);
			ImGui::Text("Filter");
			ImGui::SameLine();
			ImGui::InputText("Filter", explorer.filter);
		}

		ImGui::Separator();

		render_assets(editor, explorer, explorer.filter, &deselect);

		if (open_create_asset && deselect) ImGui::OpenPopup("CreateAsset");

		if (ImGui::BeginPopup("CreateAsset")) {
			if (ImGui::MenuItem("Import")) {
				string_buffer filename = open_dialog(window);
				if (filename != "") import_filename(editor, world, *this, filename);
			}

			if (ImGui::MenuItem("New Material")) {
				make_new_material(*this, editor);
			}

			if (ImGui::MenuItem("New Shader")) {
				make_new_shader(world, *this, editor);
			}

			if (ImGui::MenuItem("New Folder")) {
				AssetNode asset(AssetNode::Folder);
				asset.folder.name = "Empty folder";
				asset.folder.owner = explorer.current_folder;

				add_asset_to_current_folder(*this, std::move(asset));
			}

			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("EditAsset")) {
			if (ImGui::MenuItem("Delete")) { //todo right clicking will not select
				AssetNode* node = get_asset(info, explorer.selected);
				free_asset(explorer.preview_resources, *node);
				remove_from_folder(info, explorer.current_folder, node);
			}

			ImGui::EndPopup();
		}
	}

	ImGui::End();

	if (ImGui::Begin("Asset Settings")) {
		if (explorer.selected.id != INVALID_HANDLE) {
			AssetNode* node = get_asset(info, explorer.selected);
			asset_properties(*node, editor, *this);
		}
	}

	ImGui::End();
}

#include "generated.h"

/*
void save_previews_to_image(AssetPreviewResources& resources) {
	CommandPool& pool = rhi.background_graphics;

	CommandBuffer& cmd_buffer = ;

	QueueSubmitInfo info = {};
	info.cmd_buffers;
}*/

bool save_asset_info(AssetPreviewResources& resources, AssetInfo& info, SerializerBuffer& buffer, const char** err) {
	write_AssetNode_to_buffer(buffer, info.toplevel);
	write_uint_to_buffer(buffer, info.free_ids.length);
	write_n_to_buffer(buffer, info.free_ids.data, sizeof(uint) * info.free_ids.length);
	write_uint_to_buffer(buffer, resources.free_atlas_slot.length);
	write_n_to_buffer(buffer, resources.free_atlas_slot.data, sizeof(glm::vec2) * resources.free_atlas_slot.length);

	return true;
}

struct MaterialLoadJob {
	material_handle handle;
	MaterialDesc* desc = nullptr;
};

struct ModelLoadJob {
	model_handle handle;
	glm::mat4 model;
	string_view path;
};

struct LoadJobs {
	tvector<TextureLoadJob> textures;
	tvector<MaterialLoadJob> materials;
	tvector<ModelLoadJob> models;
};

//todo load shader graph!
void recursively_load_asset_node(AssetPreviewResources& resources, AssetInfo& info, LoadJobs& jobs, AssetNode& node) {
	register_asset_ptr(info, &node);

	switch (node.type) {
	case AssetNode::Texture: 
		jobs.textures.append({node.texture.handle, node.texture.path});
		break;

	case AssetNode::Model: {
		resources.render_preview_for.append(node.handle);
		glm::mat4 mat4 = compute_model_matrix(node.model.trans);
		jobs.models.append({ node.model.handle, mat4, node.model.path });

		break;
	}

	case AssetNode::Material: {
		resources.render_preview_for.append(node.handle);
		jobs.materials.append({ node.material.handle, &node.material.desc });
		break;
	}

	case AssetNode::Folder:
		for (AssetNode& children : node.folder.contents) {
			recursively_load_asset_node(resources, info, jobs, children);
		}
	
		break;
	}	
}

bool load_asset_info(AssetPreviewResources& resources, AssetInfo& info, DeserializerBuffer& buffer, const char** err) {
	LoadJobs jobs;
	
	read_AssetNode_from_buffer(buffer, info.toplevel);
	read_uint_from_buffer(buffer, info.free_ids.length);
	info.free_ids.resize(info.free_ids.length);

	read_n_from_buffer(buffer, info.free_ids.data, sizeof(asset_handle) * info.free_ids.length);

	read_uint_from_buffer(buffer, resources.free_atlas_slot.length);
	read_n_from_buffer(buffer, resources.free_atlas_slot.data, sizeof(glm::vec2) * resources.free_atlas_slot.length);

	recursively_load_asset_node(resources, info, jobs, info.toplevel);
	
	load_TextureBatch(jobs.textures);

	if (jobs.materials.length > 0) {
		info.default_material = jobs.materials[0].handle;
	}
	else {
		//todo CREATE DEFAULT MATERIAL
	}

	for (MaterialLoadJob& job : jobs.materials) {
		make_Material(job.handle, *job.desc);
		//render_preview_for(resources, info.asset_type_handle_to_node[AssetNode::Material][job.handle.id]->material);
	}

	for (ModelLoadJob& job : jobs.models) {
		ModelAsset& asset = info.asset_type_handle_to_node[AssetNode::Model][job.handle.id]->model;

		load_Model(job.handle, job.path, job.model, asset.lod_distance);		
	}

	return true;
}

