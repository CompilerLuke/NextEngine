#include "engine/engine.h"
#include "assetTab.h"
#include "editor.h"
#include "core/io/logger.h"
#include <commdlg.h>
#include <thread>
#include "graphics/rhi/window.h"
#include "core/io/vfs.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "graphics/assets/material.h"
#include "graphics/rhi/draw.h"
#include "components/transform.h"
#include "components/camera.h"
#include <glm/glm.hpp>
#include <locale>
#include <codecvt>
#include "graphics/rhi/primitives.h"
#include "graphics/assets/material.h"
#include "custom_inspect.h"
#include "core/io/input.h"
#include "components/lights.h"
#include "core/serializer.h"
#include <thread>
#include <mutex>
#include "diffUtil.h"
#include "graphics/renderer/renderer.h"
#include <imgui/imgui.h>

#include "graphics/rhi/draw.h"
#include "graphics/rhi/pipeline.h"

//todo move this to the platform code
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <WinBase.h>
#include <stdlib.h>

#include <iostream>

#undef max

REFLECT_STRUCT_BEGIN(AssetFolder)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_MEMBER(contents)
REFLECT_STRUCT_MEMBER(owner)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(TextureAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_MEMBER(handle)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(ShaderAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_MEMBER(handle)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(ModelAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_MEMBER(handle)
REFLECT_STRUCT_MEMBER(materials)
REFLECT_STRUCT_MEMBER(trans)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(MaterialAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_MEMBER(handle)
REFLECT_STRUCT_END()

DEFINE_APP_COMPONENT_ID(AssetFolder, 0)
DEFINE_APP_COMPONENT_ID(TextureAsset, 1)
DEFINE_APP_COMPONENT_ID(ShaderAsset, 2)
DEFINE_APP_COMPONENT_ID(ModelAsset, 3)
DEFINE_APP_COMPONENT_ID(MaterialAsset, 4)

REFLECT_STRUCT_BEGIN(asset_handle)
REFLECT_STRUCT_MEMBER(id)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(AssetNode)
REFLECT_STRUCT_END()

const uint ATLAS_PREVIEWS_WIDTH = 10;
const uint ATLAS_PREVIEWS_HEIGHT = 10;

const uint ATLAS_PREVIEW_RESOLUTION = 128;

AssetNode::AssetNode() {}
AssetNode::AssetNode(Type type) {
	memset(this, 0, sizeof(AssetNode));
	this->type = type;

	if (type == Shader) shader.shader_arguments.allocator = &default_allocator;
	if (type == Model) model.materials.allocator = &default_allocator;
	if (type == Folder) folder.contents.allocator = &default_allocator;
}

void AssetNode::operator=(AssetNode&& other) {
	memcpy(this, &other, sizeof(AssetNode));
	memset(&other, 0, sizeof(AssetNode));
}

AssetNode::~AssetNode() {
	if (type == Shader) shader.shader_arguments.~vector();
	if (type == Model) model.materials.~vector();
}

void register_asset_ptr(AssetInfo& tab, AssetNode* node) {
	tab.asset_handle_to_node[node->handle.id] = node;
	
	if (node->type != AssetNode::Folder) tab.asset_type_handle_to_node[node->type][node->asset.handle] = node;
}

AssetNode* get_asset(AssetInfo& tab, asset_handle handle) {
	return tab.asset_handle_to_node[handle.id];
}

void add_to_folder(AssetInfo& self, asset_handle folder_handle, AssetNode&& asset) {
	AssetFolder& folder = get_asset(self, folder_handle)->folder;

	//array resize might  invalidating ptrs
	folder.contents.append(std::move(asset));

	for (AssetNode& asset : folder.contents) {
		register_asset_ptr(self, &asset);
	}
}

void remove_from_folder(AssetInfo& self, asset_handle folder_handle, AssetNode& asset) {
	//todo assert for folder
	AssetFolder& folder = get_asset(self, folder_handle)->folder;

	uint index = &asset - folder.contents.data;
	assert(index < folder.contents.length);

	for (uint i = index; i < folder.contents.length - 1; i++) {
		folder.contents[i] = std::move(folder.contents[i + 1]);
	}

	folder.contents.length--;
}

asset_handle make_asset_handle(AssetInfo& info) {
	return {++info.id_counter};
}

asset_handle add_asset_to_current_folder(AssetTab& tab, AssetNode&& asset) {
	asset_handle handle = make_asset_handle(tab.info);
	asset.handle = handle;
	tab.render_preview_for.append(handle);

	if (asset.type == AssetNode::Material) asset.material.rot_preview.preview_tex_coord = tab.preview_resources.free_atlas_slot.pop();
	if (asset.type == AssetNode::Model)  asset.model.rot_preview.preview_tex_coord = tab.preview_resources.free_atlas_slot.pop();
	if (asset.type == AssetNode::Shader)  asset.model.rot_preview.preview_tex_coord = tab.preview_resources.free_atlas_slot.pop();

	add_to_folder(tab.info, tab.explorer.current_folder, std::move(asset));
	return handle;
}

void render_name(sstring& name, ImFont* font) {
	ImGui::PushItemWidth(-1);
	ImGui::PushID((void*)&name);
	if (font) ImGui::PushFont(font);
	ImGui::InputText("##", name);
	ImGui::PopItemWidth();
	if (font) ImGui::PopFont();
	ImGui::PopID();
}

wchar_t* to_wide_char(const char* orig);

bool filtered(AssetExplorer& self, string_view name) {
	return !name.starts_with_ignore_case(self.filter);
}

void render_asset(AssetExplorer& tab, AssetNode& node, texture_handle tex_handle, bool* deselect, glm::vec2 uv0 = glm::vec2(0,0), glm::vec2 uv1 = glm::vec2(1,1)) {	
	if (filtered(tab, node.asset.name)) return;
	
	bool selected = node.handle.id == tab.selected.id;
	if (selected) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3);


	bool is_selected = ImGui::ImageButton(tex_handle, ImVec2(128, 128), uv0, uv1);
	if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[0]) {
		tab.selected = node.handle;
		*deselect = false;
		ImGui::OpenPopup("EditAsset");
	}

	if (selected) ImGui::PopStyleVar(1);


	if (ImGui::BeginDragDropSource()) {

		ImGui::Image(tex_handle, ImVec2(128, 128));
		ImGui::SetDragDropPayload(drop_types[node.type], &node.asset.handle, sizeof(uint));
		ImGui::EndDragDropSource();
	}
	else if (is_selected) {
		tab.selected = node.handle;
	}

	render_name(node.asset.name, tab.filename_font);

	ImGui::NextColumn();
}

void preview_from_atlas(AssetPreviewResources& resources, RotatablePreview& preview, texture_handle* handle, glm::vec2* uv0, glm::vec2* uv1) {
	*uv0 = preview.preview_tex_coord;
	*uv1 = *uv0 + glm::vec2(1.0 / ATLAS_PREVIEWS_WIDTH, 1.0 / ATLAS_PREVIEWS_HEIGHT);
	*handle = resources.preview_atlas;
}

void preview_image(AssetPreviewResources& resources, RotatablePreview& preview, glm::vec2 size) {
	texture_handle preview_atlas;
	glm::vec2 uv0, uv1;

	preview_from_atlas(resources, preview, &preview_atlas, &uv0, &uv1);
	ImGui::Image(preview_atlas, size, uv0, uv1);
}

void render_asset(AssetExplorer& tab, AssetNode& node, RotatablePreview& preview, bool* deselect) {
	texture_handle handle;
	glm::vec2 uv0, uv1;

	preview_from_atlas(tab.preview_resources, preview, &handle, &uv0, &uv1);
	render_asset(tab, node, tab.preview_resources.preview_atlas, deselect, uv0, uv1);
}



void move_to_folder(AssetInfo& self, asset_handle src_folder, asset_handle dst_folder, AssetNode&& asset) {
	add_to_folder(self, dst_folder, std::move(asset));
	remove_from_folder(self, src_folder, asset);
}

void accept_move_to_folder(AssetInfo& tab, AssetNode** ptr) {
	for (uint type = 0; type < AssetNode::Type::Count; type++) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(drop_types[type])) {
			uint id = *(uint*)payload->Data;
			*ptr = tab.asset_type_handle_to_node[type][id];
		}
	}
}

struct MoveToFolder {
	AssetNode* asset = NULL;
	asset_handle dst_folder = { INVALID_HANDLE };
	asset_handle src_folder = { INVALID_HANDLE };
};

void folder_move_drop_target(AssetInfo& tab, MoveToFolder& move_to_folder, asset_handle src_folder, asset_handle dst_folder) {
	if (ImGui::BeginDragDropTarget()) {
		accept_move_to_folder(tab, &move_to_folder.asset);
		move_to_folder.src_folder = src_folder;
		move_to_folder.dst_folder = dst_folder;
		ImGui::EndDragDropTarget();
	}
}

void move_dropped_to_folder(AssetInfo& tab, MoveToFolder& move) {
	if (move.dst_folder.id != INVALID_HANDLE) move_to_folder(tab, move.src_folder, move.dst_folder, std::move(*move.asset));
}

void render_assets(Editor& editor, AssetExplorer& asset_tab, string_view filter, bool* deselect) {
	asset_handle folder_handle = asset_tab.current_folder;
	AssetFolder& folder = get_asset(asset_tab.info, folder_handle)->folder;
	
	ImGui::PushStyleColor(ImGuiCol_Column, ImVec4(0.16, 0.16, 0.16, 1));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16, 0.16, 0.16, 1));

	ImGui::Columns(16, 0, false);

	MoveToFolder move_to_folder = {};

	for (AssetNode& node : folder.contents) {
		if (node.type == AssetNode::Texture) render_asset(asset_tab, node, node.texture.handle, deselect);
		if (node.type == AssetNode::Shader) render_asset(asset_tab, node, editor.get_icon("shader"), deselect);
		if (node.type == AssetNode::Model) render_asset(asset_tab, node, node.model.rot_preview, deselect);
		if (node.type == AssetNode::Material) render_asset(asset_tab, node, node.material.rot_preview, deselect);

		if (node.type == AssetNode::Folder && !filtered(asset_tab, node.folder.name)) {
			ImGui::PushID(node.handle.id);
			if (ImGui::ImageButton(editor.get_icon("folder"), ImVec2(128, 128))) {
				asset_tab.current_folder = node.handle;
				*deselect = false;
			}
			ImGui::PopID();

			if (ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("DRAG_AND_DROP_FOLDER", &node.handle.id, sizeof(uint));
				ImGui::Image(editor.get_icon("folder"), ImVec2(128, 128));
				ImGui::EndDragDropSource();
			}

			folder_move_drop_target(asset_tab.info, move_to_folder, asset_tab.current_folder, folder_handle);

			render_name(node.folder.name, asset_tab.filename_font);
			ImGui::NextColumn();
		}
	}

	ImGui::Columns(1);
	ImGui::PopStyleColor(2);

	move_dropped_to_folder(asset_tab.info, move_to_folder);
}

void asset_properties(TextureAsset* tex, Editor& editor, AssetTab& self, RenderPass& ctx) {

}

//Materials


string_buffer open_dialog(Window& window) {
	string_view asset_folder_path = current_asset_path_folder();
	char filename[MAX_PATH];

	OPENFILENAME ofn;
	memset(&filename, 0, sizeof(filename));
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);

	ofn.hwndOwner = (HWND)window.get_win32_window();
	ofn.lpstrFilter = "All Files\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
	ofn.lpstrDefExt = "";
	ofn.lpstrInitialDir = asset_folder_path.c_str();
	
	bool success = GetOpenFileName(&ofn);

	if (success) {
		return filename;
	}

	return "";
}

MaterialDesc base_shader_desc(shader_handle shader) {
	MaterialDesc desc{ shader };
	desc.draw_state = Cull_None;

	mat_channel3(desc, "diffuse", glm::vec3(1.0f), load_Texture("wood_2/Stylized_Wood_basecolor.jpg"));
	mat_channel1(desc, "metallic", 1.0f, load_Texture("wood_2/Stylized_Wood_metallic.jpg"));
	mat_channel1(desc, "roughness", 0.2f, load_Texture("wood_2/Stylized_Wood_roughness.jpg"));
	mat_channel1(desc, "normal", 1.0f, load_Texture("wood_2/Stylized_Wood_normal.jpg"));
	mat_vec2(desc, "tiling", glm::vec2(5.0f));
	//mat_vec2(desc, "tiling", glm::vec2(1, 1));

	return desc;
}

material_handle create_default_material() { //todo move to materialSystem
	MaterialDesc desc = base_shader_desc(load_Shader("shaders/pbr.vert", "shaders/pbr.frag"));
	return make_Material(desc);
}

material_handle create_new_material(AssetTab& self, Editor& editor) {
	material_handle handle = create_default_material();
	
	AssetNode asset(AssetNode::Material);
	asset.material.handle = handle;
	asset.material.name = "Empty";

	add_asset_to_current_folder(self, std::move(asset));
	
	return handle;
}

RenderPass begin_preview_pass(AssetPreviewResources& self, Camera& camera, Transform& trans) {
	RenderPass render_pass = begin_render_pass(PreviewPass);
	bind_vertex_buffer(render_pass.cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
	bind_pipeline_layout(render_pass.cmd_buffer, self.pipeline_layout);
	bind_descriptor(render_pass.cmd_buffer, 0, self.pass_descriptor);
	bind_descriptor(render_pass.cmd_buffer, 1, self.pbr_descriptor);

	update_camera_matrices(trans, camera, render_pass.viewport);

	PassUBO pass_ubo;
	fill_pass_ubo(pass_ubo, render_pass.viewport);
	memcpy_ubo_buffer(self.pass_ubo, &pass_ubo);

	return render_pass;
}

void end_preview_pass(AssetPreviewResources& self, RenderPass& render_pass, glm::vec2 copy_preview_to) {
	end_render_pass(render_pass);
	
	ImageOffset src_region[2] = { 0,0,render_pass.viewport.width, render_pass.viewport.height };
	ImageOffset dst_region[2];
	dst_region[0].x = copy_preview_to.x * ATLAS_PREVIEWS_WIDTH * ATLAS_PREVIEW_RESOLUTION;
	dst_region[0].y = copy_preview_to.y * ATLAS_PREVIEWS_HEIGHT * ATLAS_PREVIEW_RESOLUTION;
	dst_region[1].x = dst_region[0].x + ATLAS_PREVIEW_RESOLUTION;
	dst_region[1].y = dst_region[0].y + ATLAS_PREVIEW_RESOLUTION;

	if (self.atlas_layout_undefined) {
		transition_layout(render_pass.cmd_buffer, self.preview_atlas, TextureLayout::Undefined, TextureLayout::TransferDstOptimal);
		self.atlas_layout_undefined = false;
	}
	else {
		transition_layout(render_pass.cmd_buffer, self.preview_atlas, TextureLayout::ShaderReadOptimal, TextureLayout::TransferDstOptimal);
	}

	transition_layout(render_pass.cmd_buffer, self.preview_map, TextureLayout::ShaderReadOptimal, TextureLayout::TransferSrcOptimal);
	blit_image(render_pass.cmd_buffer, Filter::Linear, self.preview_map, src_region, self.preview_atlas, dst_region);
	transition_layout(render_pass.cmd_buffer, self.preview_atlas, TextureLayout::TransferDstOptimal, TextureLayout::ShaderReadOptimal);
	transition_layout(render_pass.cmd_buffer, self.preview_map, TextureLayout::TransferSrcOptimal, TextureLayout::ShaderReadOptimal);
}

void render_preview_for(AssetPreviewResources& self, ModelAsset& asset) {	
	Camera camera;
	Transform camera_trans, trans_of_model;
	trans_of_model.rotation = asset.rot_preview.rot;
	
	Model* model = get_Model(asset.handle);
	AABB aabb = model->aabb;

	glm::vec3 center = (aabb.max + aabb.min) / 2.0f;
	float radius = 0;

	glm::vec4 verts[8];
	aabb_to_verts(&aabb, verts);
	for (int i = 0; i < 8; i++) {
		radius = glm::max(radius, glm::length(glm::vec3(verts[i]) - center));
	}
	
	double fov = glm::radians(60.0f);

	camera.fov = 60.0f; // fov;
	camera_trans.position = center;
	camera_trans.position.z += (radius) / glm::tan(fov / 2.0);

	RenderPass render_pass = begin_preview_pass(self, camera, camera_trans);

	draw_mesh(render_pass.cmd_buffer, asset.handle, asset.materials, trans_of_model);

	end_preview_pass(self, render_pass, asset.rot_preview.preview_tex_coord);
}


void render_preview_for(AssetPreviewResources& self, MaterialAsset& asset) {
	Camera camera;
	Transform camera_trans;
	camera_trans.position.z = 2.5;

	Transform trans_of_sphere;
	trans_of_sphere.rotation = asset.rot_preview.rot;

	RenderPass render_pass = begin_preview_pass(self, camera, camera_trans);

	draw_mesh(render_pass.cmd_buffer, primitives.sphere, asset.handle, trans_of_sphere);

	end_preview_pass(self, render_pass, asset.rot_preview.preview_tex_coord);
}

void rot_preview(AssetPreviewResources& resources, RotatablePreview& self) {
	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Preview");

	ImGui::Image(resources.preview_map, ImVec2(512, 512));
	
	if (ImGui::IsItemHovered() && ImGui::IsMouseDragging()) {
		self.previous = self.current;
		self.current = glm::vec2(ImGui::GetMouseDragDelta().x, ImGui::GetMouseDragDelta().y);

		glm::vec2 diff = self.current - self.previous;

		self.rot_deg += diff;
		self.rot = glm::quat(glm::vec3(glm::radians(self.rot_deg.y), 0, 0)) * glm::quat(glm::vec3(0, glm::radians(self.rot_deg.x), 0));
	}
	else {
		self.previous = glm::vec2(0);
	}

	//todo copy preview map into preview atlas
}


void edit_color(glm::vec4& color, string_view name, glm::vec2 size) {

	ImVec4 col(color.x, color.y, color.z, color.w);
	if (ImGui::ColorButton(name.c_str(), col, 0, ImVec2(size.x, size.y))) {
		ImGui::OpenPopup(name.c_str());
	}
	
	color = glm::vec4(col.x, col.y, col.z, col.w);

	if (ImGui::BeginPopup(name.c_str())) {
		ImGui::ColorPicker4(name.c_str(), &color.x);
		ImGui::EndPopup();
	}
}

void edit_color(glm::vec3& color, string_view name, glm::vec2 size) {
	glm::vec4 color4 = glm::vec4(color, 1.0f);
	edit_color(color4, name, size);
	color = color4;
}

void channel_image(uint& image, string_view name) {
	static texture_handle tex = load_Texture("solid_white.png");
	
	if (image == INVALID_HANDLE) { ImGui::Image(tex, ImVec2(200, 200)); }
	else ImGui::Image({ image }, ImVec2(200, 200));
	
	if (ImGui::IsMouseDragging() && ImGui::IsItemHovered()) {
		image = INVALID_HANDLE;
	}

	accept_drop("DRAG_AND_DROP_IMAGE", &image, sizeof(texture_handle));
	ImGui::SameLine();

	ImGui::Text(name.c_str());
	ImGui::SameLine(ImGui::GetWindowWidth() - 300);
}

void set_params_for_shader_graph(AssetTab& asset_tab, shader_handle shader);

//todo refactor Editor into smaller chunks
void inspect_material_params(Editor& editor, MaterialDesc* material) {	
	for (auto& param : material->params) {
		DiffUtil diff_util;
		begin_tdiff(diff_util, &param);

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
			ImGui::Image(texture_handle{ param.image }, ImVec2(200, 200));

			accept_drop("DRAG_AND_DROP_IMAGE", &param.image, sizeof(texture_handle));

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
		}

		if (param.type == Param_Channel3) {


	
			channel_image(param.image, name);
			edit_color(param.vec3, name, glm::vec2(50, 50));
		}

		if (param.type == Param_Channel2) {
			channel_image(param.image, name);
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::InputFloat2(tformat("##", name).c_str(), &param.vec2.x);
		}

		if (param.type == Param_Channel1) {
			channel_image(param.image, name);
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::SliderFloat(tformat("##", name).c_str(), &param.real, 0, 1.0f);
		}

		if (end_diff(editor.actions, diff_util, "Material Property")) {
			
		}
	}
}

void asset_properties(MaterialAsset* mat_asset, Editor& editor, AssetTab& self, RenderPass& ctx) {
	auto& name = mat_asset->name;

	render_name(name, self.explorer.default_font);

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Material");

	MaterialDesc* desc = material_desc(mat_asset->handle);
	if (desc == NULL) {
		ImGui::Text("Material Handle is INVALID");
	}

	ShaderInfo* info = shader_info(desc->shader);
	if (info == NULL) {
		ImGui::Text("Shader Handle is INVALID!");
	}

	if (ImGui::Button(info->ffilename.data)) {
		ImGui::OpenPopup("StandardShaders");
	}


	if (accept_drop("DRAG_AND_DROP_SHADER", &desc->shader, sizeof(shader_handle))) {
		set_params_for_shader_graph(self, desc->shader);
	}

	
	if (ImGui::BeginPopup("StandardShaders")) {
		if (ImGui::Button("shaders/pbr.frag")) {
			shader_handle new_shad = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");
			if (new_shad.id != desc->shader.id) {
				//set_base_shader_params(assets, desc, new_shad);
				//material->set_vec2(shader_manager, "transformUVs", glm::vec2(1, 1));
			}
		}

		if (ImGui::Button("shaders/tree.frag")) {
			shader_handle new_shad = load_Shader("shaders/tree.vert", "shaders/tree.frag");
			if (new_shad.id != desc->shader.id) {
				//set_base_shader_params(texture_manager, shader_manager, material, new_shad);
				//material->set_float(shader_manager, "cutoff", 0.1f),
				//material->set_vec2(shader_manager, "transformUVs", glm::vec2(1, 1));
			}
		}

		if (ImGui::Button("shaders/paralax_pbr.frag")) {
			shader_handle new_shad = load_Shader("shaders/pbr.vert", "shaders/paralax_pbr.frag");

			if (new_shad.id != desc->shader.id) {
				//set_base_shader_params(texture_manager, shader_manager, material, new_shad);
				//material->set_image(shader_manager, "material.height", texture_manager.load("black.png"));
				//material->set_image(shader_manager, "material.ao", texture_manager.load("solid_white.png")),
				//	material->set_int(shader_manager, "steps", 5);
				//material->set_float(shader_manager, "depth_scale", 1);
				//material->set_vec2(shader_manager, "transformUVs", glm::vec2(1, 1));
			}
		}
		
		ImGui::EndPopup();
	}

	inspect_material_params(editor, desc);
	
	rot_preview(self.preview_resources, mat_asset->rot_preview);
	render_preview_for(self.preview_resources, *mat_asset);
}

void asset_properties(ModelAsset* mod_asset, Editor& editor, AssetTab& self, RenderPass& ctx) {
	Model* model = get_Model(mod_asset->handle);

	render_name(mod_asset->name, self.explorer.default_font);

	DiffUtil diff_util;
	begin_tdiff(diff_util, &mod_asset->trans);

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Transform");
	ImGui::InputFloat3("position", &mod_asset->trans.position.x);
	get_on_inspect_gui("glm::quat")(&mod_asset->trans.rotation, "rotation", editor);
	ImGui::InputFloat3("scale", &mod_asset->trans.scale.x);

	end_diff(editor.actions, diff_util, "Properties Material");

	if (ImGui::Button("Apply")) {
		load_Model(mod_asset->handle, mod_asset->trans.compute_model_matrix());

		mod_asset->rot_preview.rot_deg = glm::vec2();
		mod_asset->rot_preview.current = glm::vec2();
		mod_asset->rot_preview.previous = glm::vec2();
		mod_asset->rot_preview.rot = glm::quat();
	}

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Materials");

	for (int i = 0; i < mod_asset->materials.length; i++) {
		string_buffer prefix = tformat(model->materials[i], " : ");
		
		get_on_inspect_gui("Material")(&mod_asset->materials[i], prefix, editor);
	}

	end_diff(editor.actions, diff_util, "Asset Properties");

	rot_preview(self.preview_resources, mod_asset->rot_preview);
	render_preview_for(self.preview_resources, *mod_asset);
}

sstring name_from_filename(string_view filename) {
	int after_slash = filename.find_last_of('\\') + 1;
	return filename.sub(after_slash, filename.find_last_of('.'));
}

model_handle import_model(AssetTab& self, string_view filename) {	
	model_handle handle = load_Model(filename, true);
	Model* model = get_Model(handle);

	AssetNode asset(AssetNode::Model); 
	asset.model.handle = handle;

	for (sstring& material : model->materials) {
		asset.model.materials.append(self.default_material);
	}

	asset.model.name = name_from_filename(filename);
	add_asset_to_current_folder(self, std::move(asset));

	return handle;
}

void import_texture(Editor& editor, AssetTab& self, string_view filename) {
	texture_handle handle = load_Texture(filename, true);

	AssetNode asset(AssetNode::Texture);
	asset.texture.handle = handle;
	asset.texture.name = name_from_filename(filename);

	add_asset_to_current_folder(self, std::move(asset));
}

void import_shader(Editor& editor, AssetTab& self, string_view filename) {
	return; //
	/*
	string_buffer frag_filename(filename.sub(0, filename.length - strlen(".vert")));
	frag_filename += ".frag";

	shader_handle handle = self.assets.shaders.load(filename, frag_filename);
	ID id = self.assets.make_ID();
	ShaderAsset* shader_asset = self.assets.make<ShaderAsset>(id);

	add_asset(self, id);
	*/
}

//todo revise this function
void import_filename(Editor& editor, World& world, RenderPass& ctx, AssetTab& self, string_view filename) {
	string_buffer asset_path;
	if (!asset_path_rel(filename, &asset_path)) {
		asset_path = filename.sub(filename.find_last_of('\\') + 1, filename.size());

		string_buffer new_filename = tasset_path(asset_path);
		if (!CopyFile(filename.c_str(), new_filename.c_str(), true)) {
			log("Could not copy filename");
		}
	}

	log(asset_path);

	if (asset_path.ends_with(".fbx") || asset_path.ends_with(".FBX")) {
		import_model(self, asset_path);
	}
	else if (asset_path.ends_with(".jpg") || asset_path.ends_with(".png")) {
		import_texture(editor, self, asset_path);
	}
	else if (asset_path.ends_with(".vert") || asset_path.ends_with(".frag")) {
		import_shader(editor, self, asset_path);
	}
	else {
		log("Unsupported extension for", asset_path);
	}
}

void make_AssetPreviewRenderData(AssetPreviewResources& resources, Renderer& renderer) {
	resources.pbr_descriptor = renderer.lighting_system.pbr_descriptor;
	
	{
		FramebufferDesc desc{ 512, 512 };
		AttachmentDesc& color_attachment = add_color_attachment(desc, &resources.preview_map);
		//color_attachment.final_layout = TextureLayout::TransferSrcOptimal;
		color_attachment.usage |= TextureUsage::TransferSrc;

		make_Framebuffer(PreviewPass, desc);
	}

	{
		FramebufferDesc desc{ 512, 512 };
		add_color_attachment(desc, &resources.preview_tonemapped_map);
		make_Framebuffer(PreviewPassTonemap, desc);
	}

	{
		resources.pass_ubo = alloc_ubo_buffer(sizeof(PassUBO), UBO_PERMANENT_MAP);

		DescriptorDesc descriptor_desc = {};
		add_ubo(descriptor_desc, VERTEX_STAGE, resources.pass_ubo, 0);
		update_descriptor_set(resources.pass_descriptor, descriptor_desc);

		array<2, descriptor_set_handle> descriptor_layouts = { resources.pass_descriptor, resources.pbr_descriptor };
		resources.pipeline_layout = query_Layout(descriptor_layouts);
	}


	for (uint h = 0; h < ATLAS_PREVIEWS_HEIGHT; h++) {
		for (uint w = 0; w < ATLAS_PREVIEWS_WIDTH; w++) {
			glm::vec2 tex_coord((float)w / ATLAS_PREVIEWS_WIDTH, (float)h / ATLAS_PREVIEWS_HEIGHT);
			resources.free_atlas_slot.append(tex_coord);
		}
	}

	TextureDesc desc = {};
	desc.width = ATLAS_PREVIEWS_WIDTH * ATLAS_PREVIEW_RESOLUTION;
	desc.height = ATLAS_PREVIEWS_HEIGHT * ATLAS_PREVIEW_RESOLUTION;
	desc.num_channels = 4;

	resources.preview_atlas = alloc_Texture(desc);
}

void make_AssetInfo(AssetInfo& info) {
	info.toplevel = AssetNode(AssetNode::Folder);
	info.toplevel.handle = make_asset_handle(info);
	info.toplevel.folder.name = "Assets";
	register_asset_ptr(info, &info.toplevel);
}

AssetTab::AssetTab(Renderer& renderer, AssetInfo& info, Window& window) : 
	info(info),
	window(window), 
	renderer(renderer), 
	explorer{ info, preview_resources }
{
	make_AssetPreviewRenderData(preview_resources, renderer);
	make_AssetInfo(info);

	explorer.current_folder = info.toplevel.handle;
}

//todo although it is possible to render multiple previews in one frame
//this would require several ubo buffers, possibly with push constants to get the right one!
//in theory this also has a frame sychronization issue, as it's not using multiple ubo for different frames
void render_previews(AssetTab& self) {
	while (self.render_preview_for.length > 0) {
		AssetNode* node = get_asset(self.info, self.render_preview_for.pop());

		if (node->type == AssetNode::Material) render_preview_for(self.preview_resources, node->material);
		if (node->type == AssetNode::Model) render_preview_for(self.preview_resources, node->model);

		break;
	}
	
	/*for (asset_handle handle : self.render_preview_for) {
		AssetNode* node = get_asset(self.info, handle);

		if (node->type == AssetNode::Material) render_preview_for(self.preview_resources, node->material);
		//if (node->type == AssetNode::Model) render_preview_for(self.preview_resources, node->model);

		//self.render_preview_for.pop();
		break;
	}

	self.render_preview_for.clear();*/
}

void AssetTab::render(World& world, Editor& editor, RenderPass& ctx) {
	render_previews(*this);
	
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

			for (int i = folder_file_path.length - 1; i >= 0; i--) {
				if (ImGui::Button(folder_file_path[i].c_str())) {
					explorer.current_folder = folder_id[i];
				}

				folder_move_drop_target(info, move_to_folder, explorer.current_folder, folder_id[i]);

				ImGui::SameLine();
				ImGui::Text("\\");
				ImGui::SameLine();
			}

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

		if (ImGui::BeginPopup("CreateAsset"))
		{
			if (ImGui::MenuItem("Import")) {
				string_buffer filename = open_dialog(window);
				if (filename != "") import_filename(editor, world, ctx, *this, filename);
			}

			if (ImGui::MenuItem("New Material"))
			{
				create_new_material(*this, editor);
			}

			if (ImGui::MenuItem("New Shader"))
			{
				create_new_shader(world, *this, editor);
			}

			if (ImGui::MenuItem("New Folder"))
			{
				AssetNode asset(AssetNode::Folder);
				asset.folder.name = "Empty folder";
				asset.folder.owner = explorer.current_folder;

				add_asset_to_current_folder(*this, std::move(asset));
			}

			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("EditAsset"))
		{
			if (ImGui::MenuItem("Delete")) { //todo right clicking will not select
				remove_from_folder(info, explorer.current_folder, *get_asset(info, explorer.selected));
			}

			ImGui::EndPopup();
		}
	}

	ImGui::End();

	if (ImGui::Begin("Asset Settings")) {
		if (explorer.selected.id != INVALID_HANDLE) {
			AssetNode* node = get_asset(info, explorer.selected);
			
			if (node->type == AssetNode::Texture) asset_properties(&node->texture, editor, *this, ctx);
			if (node->type == AssetNode::Shader) asset_properties(&node->shader, editor, *this, ctx);
			if (node->type == AssetNode::Model) asset_properties(&node->model, editor, *this, ctx);
			if (node->type == AssetNode::Material) asset_properties(&node->material, editor, *this, ctx);
		}
	}

	ImGui::End();
}

template<typename T>
struct TaskList {
	std::mutex mutex;
	vector<T> tasks;

	void append() {

	}
	bool pop();
};

unsigned int read_save_file(string_view filename, DeserializerBuffer* serializer, string_buffer* buffer) {
	if (!io_readfb(filename, buffer)) throw "Could not read serialization file!";
	
	new (serializer) DeserializerBuffer(buffer->data, buffer->length);
	return serializer->read_int();
}

/*
REFLECT_UNION_BEGIN(Param)
REFLECT_UNION_FIELD(loc)
REFLECT_UNION_CASE_BEGIN()
REFLECT_UNION_CASE(vec3)
REFLECT_UNION_CASE(vec2)
REFLECT_UNION_CASE(matrix)
REFLECT_UNION_CASE(image)
REFLECT_UNION_CASE(cubemap)
REFLECT_UNION_CASE(integer)
REFLECT_UNION_CASE(real)
REFLECT_UNION_CASE(channel3)
REFLECT_UNION_CASE(channel2)
REFLECT_UNION_CASE(channel1)
REFLECT_UNION_CASE(time)
REFLECT_UNION_END()

REFLECT_STRUCT_BEGIN(Param::Channel3)
REFLECT_STRUCT_MEMBER(image)
REFLECT_STRUCT_MEMBER(color)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Param::Channel2)
REFLECT_STRUCT_MEMBER(image)
REFLECT_STRUCT_MEMBER(value)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Param::Channel1)
REFLECT_STRUCT_MEMBER(image)
REFLECT_STRUCT_MEMBER(value)
REFLECT_STRUCT_END()


void convert_to_desc(Param& param, ParamDesc& param_desc) {
	param_desc.type = param.type;

	if (param_desc.type == Param_Float) {
		param_desc.real = param.real;
	}

	if (param_desc.type == Param_Int) {
		param_desc.integer = param.integer;
	}

	if (param_desc.type == Param_Vec2) {
		param_desc.vec2 = param.vec2;
	}

	if (param_desc.type == Param_Vec3) {
		param_desc.vec3 = param.vec3;
	}

	if (param_desc.type == Param_Image) {
		param_desc.image = param.image.id;
	}

	if (param_desc.type == Param_Channel1) {
		param_desc.image = param.channel1.image.id;
		param_desc.real = param.channel1.value;
	}

	if (param_desc.type == Param_Channel2) {
		param_desc.image = param.channel2.image.id;
		param_desc.vec2 = param.channel2.value;
	}

	if (param_desc.type == Param_Channel3) {
		param_desc.image = param.channel3.image.id;
		param_desc.vec3 = param.channel3.color;
	}
}*/

void AssetTab::on_load(World& world) {
	string_buffer buffer;
	buffer.allocator = &temporary_allocator;
	
	/*{
		DeserializerBuffer serializer;

		unsigned int num = read_save_file("data/AssetFolder.ne", &serializer, &buffer);

		for (int i = 0; i < num; i++) {
			ID id = serializer.read_int();

			AssetFolder folder;
			serializer.read(reflect::TypeResolver<AssetFolder>::get(), assets.make<AssetFolder>(id));
			assets.skipped_ids.append(id);
		}

		this->current_folder = 0;
	}

	{
		DeserializerBuffer serializer;
		unsigned int num = read_save_file("data/TextureAsset.ne", &serializer, &buffer);

		vector<TextureLoadJob> jobs;

		int num_tasks_left = 0;

		for (int i = 0; i < num; i++) {
			ID id = serializer.read_int();
			TextureAsset* tex_asset = assets.make<TextureAsset>(id);
			assets.skipped_ids.append(id);

			string_buffer filename;
			serializer.read_string(filename);
			serializer.read(reflect::TypeResolver<TextureAsset>::get(), tex_asset);
			
			TextureLoadJob job;
			job.handle = tex_asset->handle;
			job.path = filename.view();

			jobs.append(job);
		}

		load_TextureBatch(jobs);
	}

	{
		DeserializerBuffer serializer;
		unsigned int num = read_save_file("data/MaterialAsset.ne", &serializer, &buffer);

		auto paralax = load_Shader("shaders/pbr.vert", "shaders/paralax_pbr.frag");
		auto pbr_shader = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");
		auto tree = load_Shader("shaders/tree.vert", "shaders/tree.frag");

		for (int i = 0; i < num; i++) {
			ID id = serializer.read_int();
			MaterialAsset* mat_asset = assets.make<MaterialAsset>(id);
			assets.skipped_ids.append(id);

			MaterialDesc mat;

			string_buffer v_filename, f_filename;
			serializer.read_string(v_filename);
			serializer.read_string(f_filename);

			if (f_filename.starts_with("shaders")) {
				mat.shader = load_Shader(v_filename, f_filename);
			}

			uint length = serializer.read_int();

			for (int i = 0; i < length; i++) {
				//sstring name;
				//serializer.read_string(name);

				Param param;
				ParamDesc param_desc;
				serializer.read_string(param_desc.name);

				serializer.read(reflect::TypeResolver<Param>::get(), &param_desc);

				convert_to_desc(param, param_desc);

				//ParamDesc param_desc = *(ParamDesc*)serializer.pointer_to_n_bytes(sizeof(ParamDesc));
				mat.params.append(param_desc);
			}

			serializer.read(reflect::TypeResolver<MaterialAsset>::get(), mat_asset);

			insert_material_handle_to_asset(*this, mat_asset);

			replace_Material(mat_asset->handle, mat);

			if (i == 0) this->default_material = mat_asset->handle;

			render_preview_for(world, *this, *mat_asset);
		}
	}

	{
		DeserializerBuffer serializer;
		unsigned int num = read_save_file("data/ModelAsset.ne", &serializer, &buffer);

		for (int i = 0; i < num; i++) {
			ID id = serializer.read_int();
			ModelAsset* mod_asset = assets.make<ModelAsset>(id);
			assets.skipped_ids.append(id);

			sstring path;
			serializer.read_string(path);
			serializer.read(reflect::TypeResolver<ModelAsset>::get(), mod_asset);

			load_Model(mod_asset->handle, path, mod_asset->trans.compute_model_matrix());

			insert_model_handle_to_asset(*this, mod_asset);

			render_preview_for(world, *this, *mod_asset);
		}
	}

	return;
	{
		DeserializerBuffer serializer;
		unsigned int num = read_save_file("data/ShaderAsset.ne", &serializer, &buffer);

		for (int i = 0; i < num; i++) {
			ID id = serializer.read_int();
			ShaderAsset* shad_asset = assets.make<ShaderAsset>(id);
			assets.skipped_ids.append(id);

			deserialize_shader_asset(serializer, shad_asset);
			insert_shader_handle_to_asset(*this, shad_asset);
			
			load_Shader_for_graph(shad_asset);
		}
	}
	*/
}

#define BEGIN_SAVE(type_name) { \
SerializerBuffer serializer; \
auto filtered = assets.filter<type_name>(); \
serializer.write_int(filtered.length); \
\
for (type_name* asset : filtered) { \
	serializer.write_int(assets.id_of(asset));

#define END_SAVE(filename) } io_writef(filename, {serializer.data, serializer.index}); } 

void AssetTab::on_save() {
	/*
	BEGIN_SAVE(AssetFolder)
		serializer.write(reflect::TypeResolver<AssetFolder>::get(), asset);
	END_SAVE("data/AssetFolder.ne")
	
	//todo worry about serialization afterward
	BEGIN_SAVE(TextureAsset)
		Texture* tex = get_Texture(asset_manager, asset->handle);
		serializer.write_string(tex->filename);
		serializer.write(reflect::TypeResolver<TextureAsset>::get(), asset);
	END_SAVE("data/TextureAsset.ne")
	
	
	BEGIN_SAVE(MaterialAsset)
		MaterialDesc* mat = material_desc(asset->handle);
		ShaderInfo* shader = shader_info(mat->shader);
		if (shader == NULL) continue;

		serializer.write_string(shader->vfilename); //it could just save the shader handle
		serializer.write_string(shader->ffilename);

		serializer.write_int(mat->params.length);
	
		for (ParamDesc& param : mat->params) {
			*(ParamDesc*)serializer.pointer_to_n_bytes(sizeof(ParamDesc)) = param;
		}
	
		serializer.write(reflect::TypeResolver<MaterialAsset>::get(), asset);
	END_SAVE("data/MaterialAsset.ne")
	
	BEGIN_SAVE(ModelAsset)
		serializer.write_string(get_Model(asset->handle)->path);
		serializer.write(reflect::TypeResolver<ModelAsset>::get(), asset);
	END_SAVE("data/ModelAsset.ne")

	BEGIN_SAVE(ShaderAsset)
			serialize_shader_asset(serializer, asset);
	END_SAVE("data/ShaderAsset.ne")
	*/
}

