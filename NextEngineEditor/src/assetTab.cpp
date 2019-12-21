#include "stdafx.h"
#include "engine/engine.h"
#include "assetTab.h"
#include "editor.h"
#include "graphics/rhi.h"
#include "logger/logger.h"
#include <commdlg.h>
#include <thread>
#include "graphics/window.h"
#include "core/vfs.h"
#include "model/model.h"
#include "graphics/texture.h"
#include "graphics/shader.h"
#include "graphics/draw.h"
#include "components/transform.h"
#include "components/camera.h"
#include <glm/glm.hpp>
#include <locale>
#include <codecvt>
#include "graphics/primitives.h"
#include "graphics/materialSystem.h"
#include "custom_inspect.h"
#include "core/input.h"
#include "components/lights.h"
#include "serialization/serializer.h"
#include <thread>
#include <mutex>
#include "diffUtil.h"
#include <imgui/imgui.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <WinBase.h>
#include <stdlib.h>

#include <iostream>

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

vector<MaterialAsset*> AssetTab::material_handle_to_asset;
vector<ModelAsset*> AssetTab::model_handle_to_asset;
vector<ShaderAsset*> AssetTab::shader_handle_to_asset;

void insert_material_handle_to_asset(MaterialAsset* asset) {
	int diff = asset->handle.id - AssetTab::material_handle_to_asset.length + 1;

	for (int i = 0; i < diff; i++) {
		AssetTab::material_handle_to_asset.append(NULL);
	}
	AssetTab::material_handle_to_asset[asset->handle.id] = asset;
}

void insert_model_handle_to_asset(ModelAsset* asset) {
	int diff = asset->handle.id - AssetTab::model_handle_to_asset.length + 1;

	for (int i = 0; i < diff; i++) {
		AssetTab::model_handle_to_asset.append(NULL);
	}
	AssetTab::model_handle_to_asset[asset->handle.id] = asset;
}

void insert_shader_handle_to_asset(ShaderAsset* asset) {
	int diff = asset->handle.id - AssetTab::shader_handle_to_asset.length + 1;

	for (int i = 0; i < diff; i++) {
		AssetTab::shader_handle_to_asset.append(NULL);
	}
	AssetTab::shader_handle_to_asset[asset->handle.id] = asset;
}

void AssetTab::register_callbacks(Window& window, Editor& editor) {
	
}

void render_name(StringBuffer& name, ImFont* font) {
	ImGui::PushItemWidth(-1);
	ImGui::PushID((void*)&name);
	ImGui::PushFont(font);
	ImGui::InputText("##", name);
	ImGui::PopItemWidth();
	ImGui::PopFont();
	ImGui::PopID();

}

wchar_t* to_wide_char(const char* orig);

bool filtered(AssetTab& self, StringView name) {
	return !name.starts_with_ignore_case(self.filter);
}

void render_asset(ImTextureID texture_id, StringBuffer& name, AssetTab& tab, ID id, bool* deselect, const char* drag_drop_type = "", void* data = NULL) {
	if (filtered(tab, name)) return;
	
	bool selected = id == tab.selected;
	if (selected) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3);

	bool is_selected = ImGui::ImageButton((ImTextureID)texture_id, ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));
	if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[1]) {
		tab.selected = id;
		*deselect = false;
		ImGui::OpenPopup("EditAsset");
	}

	if (selected) ImGui::PopStyleVar(1);


	if (data && ImGui::BeginDragDropSource()) {

		ImGui::Image((ImTextureID)texture_id, ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));
		ImGui::SetDragDropPayload(drag_drop_type, data, sizeof(Handle<int>));
		ImGui::EndDragDropSource();
	}
	else if (is_selected) {
		tab.selected = id;
	}

	render_name(name, tab.filename_font);

	ImGui::NextColumn();
}

template<typename AssetType>
void move_to_folder(AssetTab& self, ID folder_handle, unsigned int internal_handle) {
	World& world = self.assets;
	auto folder = world.by_id<AssetFolder>(self.current_folder);
	auto move_to = world.by_id<AssetFolder>(folder_handle);

	vector<ID> new_contents;
	new_contents.reserve(folder->contents.length - 1);

	for (int i = 0; i < folder->contents.length; i++) {
		auto handle = folder->contents[i];
		auto mat = world.by_id<AssetType>(handle);
		if (mat && mat->handle.id == internal_handle) {
			move_to->contents.append(handle);
		} else {
			new_contents.append(folder->contents[i]);
		}
	}

	folder->contents = std::move(new_contents);
}

int accept_move_to_folder(const char* drop_type) {
	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(drop_type)) {
		return ((Handle<unsigned int>*)payload->Data)->id;
	}

	return -1;
}

struct DefferedMoveFolder {
	int mat_moved_to_folder = -1;
	int image_moved_to_folder = -1;
	int model_moved_to_folder = -1;
	int folder_moved_to_folder = -1;
	int shader_moved_to_folder = -1;
	int moved_to_folder = -1;

	void accept_drop(ID folder_handle) {
		if (ImGui::BeginDragDropTarget()) {
			mat_moved_to_folder = accept_move_to_folder("DRAG_AND_DROP_MATERIAL");
			model_moved_to_folder = accept_move_to_folder("DRAG_AND_DROP_MODEL");
			image_moved_to_folder = accept_move_to_folder("DRAG_AND_DROP_IMAGE");
			folder_moved_to_folder = accept_move_to_folder("DRAG_AND_DROP_FOLDER");
			shader_moved_to_folder = accept_move_to_folder("DRAG_AND_DROP_SHADER");

			moved_to_folder = folder_handle;
			ImGui::EndDragDropTarget();
		}
	}

	void apply(AssetTab& asset_tab) {
		if (moved_to_folder != -1) {
			if (mat_moved_to_folder != -1) move_to_folder<MaterialAsset>(asset_tab, moved_to_folder, mat_moved_to_folder);
			if (model_moved_to_folder != -1) move_to_folder<ModelAsset>(asset_tab, moved_to_folder, model_moved_to_folder);
			if (image_moved_to_folder != -1) move_to_folder<TextureAsset>(asset_tab, moved_to_folder, image_moved_to_folder);
			if (shader_moved_to_folder != -1) move_to_folder<ShaderAsset>(asset_tab, moved_to_folder, shader_moved_to_folder);
			if (folder_moved_to_folder != -1) {
				asset_tab.assets.by_id<AssetFolder>(folder_moved_to_folder - 1)->owner = moved_to_folder;
				move_to_folder<AssetFolder>(asset_tab, moved_to_folder, folder_moved_to_folder);
			}
		}
	}
};

void render_assets(Editor& editor, AssetTab& asset_tab, StringView filter, bool* deselect) {
	ID folder_handle = asset_tab.current_folder;
	World& world = asset_tab.assets;
	auto folder = world.by_id<AssetFolder>(folder_handle);
	
	ImGui::PushStyleColor(ImGuiCol_Column, ImVec4(0.16, 0.16, 0.16, 1));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16, 0.16, 0.16, 1));

	ImGui::Columns(16, 0, false);

	DefferedMoveFolder deffered_move_folder;

	for (auto handle : folder->contents) {
		auto tex = world.by_id<TextureAsset>(handle);
		auto shad = world.by_id<ShaderAsset>(handle);
		auto mod = world.by_id<ModelAsset>(handle);
		auto mat = world.by_id<MaterialAsset>(handle);
		auto folder = world.by_id<AssetFolder>(handle);

		if (tex) {
			render_asset((ImTextureID)texture::id_of(tex->handle), tex->name, asset_tab, handle, deselect, "DRAG_AND_DROP_IMAGE", &tex->handle);
		}

		if (shad) { 
			render_asset((ImTextureID)editor.get_icon("shader"), shad->name, asset_tab, handle, deselect, "DRAG_AND_DROP_SHADER", &shad->handle);
		}

		if (mod) {
			render_asset((ImTextureID)texture::id_of(mod->rot_preview.preview), mod->name, asset_tab, handle, deselect, "DRAG_AND_DROP_MODEL", &mod->handle);
		}

		if (mat) { 
			render_asset((ImTextureID)texture::id_of(mat->rot_preview.preview), mat->name, asset_tab, handle, deselect, "DRAG_AND_DROP_MATERIAL", &mat->handle);
		}

		if (folder && !filtered(asset_tab, folder->name)) {
			folder->handle = { handle + 1 };

			ImGui::PushID(handle);
			if (ImGui::ImageButton((ImTextureID)editor.get_icon("folder"), ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0))) {
				asset_tab.current_folder = handle;
				*deselect = false;
			}
			ImGui::PopID();

			if (ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("DRAG_AND_DROP_FOLDER", &folder->handle, sizeof(void*));
				ImGui::Image((ImTextureID)editor.get_icon("folder"), ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));
				ImGui::EndDragDropSource();
			}

			deffered_move_folder.accept_drop(handle);

			render_name(folder->name, asset_tab.filename_font);
			ImGui::NextColumn();
		}
	}

	ImGui::Columns(1);
	ImGui::PopStyleColor(2);

	deffered_move_folder.apply(asset_tab);
}

void asset_properties(TextureAsset* tex, Editor& editor, World& world, AssetTab& self, RenderParams& params) {

}

//Materials


std::wstring open_dialog(Editor& editor) {
	wchar_t filename[MAX_PATH];

	OPENFILENAME ofn;
	memset(&filename, 0, sizeof(filename));
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);

	ofn.hwndOwner = glfwGetWin32Window(editor.engine.window.window_ptr);
	ofn.lpstrFilter = L"All Files\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
	ofn.lpstrDefExt = L"";
	ofn.lpstrInitialDir = to_wide_char(Level::asset_folder_path.c_str());

	bool success = GetOpenFileName(&ofn);
	
	delete ofn.lpstrInitialDir;

	if (success) {
		return std::wstring(filename);
	}

	return L"";
}

void create_texture_for_preview(Handle<Texture>& preview, AssetTab& self) {
	unsigned int texture_id;
	if (preview.id == INVALID_HANDLE) {
		glGenTextures(1, &texture_id);
		glBindTexture(GL_TEXTURE_2D, texture_id);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, self.preview_fbo.width, self.preview_fbo.height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		preview = RHI::texture_manager.make({ "asset preview", texture_id });
	}
	else {
		texture::bind_to(preview, 0);
	}

	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, self.preview_fbo.width, self.preview_fbo.height);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
}

RenderParams create_preview_command_buffer(CommandBuffer& cmd_buffer, RenderParams& old_params, AssetTab& self, Camera* cam, World& world) {
	RenderParams render_params = old_params;
	render_params.width = self.preview_fbo.width;
	render_params.height = self.preview_fbo.height;
	render_params.command_buffer = &cmd_buffer;
	render_params.cam = cam;

	//Light

	{
		auto id = world.make_ID();
		auto e = world.make<Entity>(id);
		auto trans = world.make<Transform>(id);
		auto dir_light = world.make<DirLight>(id);

		render_params.dir_light = dir_light;
	}

	cam->fov = 60;

	update_camera_matrices(world, world.id_of(cam), render_params);

	return render_params;
}


void set_base_shader_params(Material* material, Handle<Shader> new_shad) {
	*material = Material(new_shad);
	material->set_image("material.diffuse", load_Texture("solid_white.png"));
	material->set_image("material.roughness", load_Texture("solid_white.png"));
	material->set_image("material.metallic", load_Texture("black.png"));
	material->set_image("material.normal", load_Texture("normal.jpg"));
}

Handle<Material> create_default_material() { //todo move to materialSystem
	Material mat;
	set_base_shader_params(&mat, load_Shader("shaders/pbr.vert", "shaders/pbr.frag"));
	mat.set_vec2("transformsUVs", glm::vec2(1, 1));

	return RHI::material_manager.make(std::move(mat), true);
}

void render_preview_to_buffer(AssetTab& self, RenderParams& params, CommandBuffer& cmd_buffer, Handle<Texture>& preview, World& world) {
	((MainPass*)params.pass)->shadow_pass.shadow_mask.shadow_mask_map_fbo.bind();
	((MainPass*)params.pass)->shadow_pass.shadow_mask.shadow_mask_map_fbo.clear_color(glm::vec4(0, 0, 0, 1));
	
	self.preview_fbo.bind();


	self.preview_fbo.clear_color(glm::vec4(0, 0, 0, 1));
	self.preview_fbo.clear_depth(glm::vec4(0, 0, 0, 1));

	Handle<Shader> tone_map = load_Shader("shaders/screenspace.vert", "shaders/preview.frag");

	params.command_buffer->submit_to_gpu(world, params);

	self.preview_fbo.unbind();
	self.preview_tonemapped_fbo.bind();
	self.preview_tonemapped_fbo.clear_color(glm::vec4(0, 0, 0, 1));
	self.preview_tonemapped_fbo.clear_depth(glm::vec4(0, 0, 0, 1));

	glm::mat4 identity(1.0);

	shader::bind(tone_map);
	shader::set_int(tone_map, "frameMap", 0);
	shader::set_mat4(tone_map, "model", identity);
	texture::bind_to(self.preview_map, 0);

	render_quad();

	create_texture_for_preview(preview, self);

	self.preview_tonemapped_fbo.unbind();

	world.free_now_by_id(world.id_of(params.dir_light));
}

void render_preview_for(World& world, AssetTab& self, ModelAsset& asset, RenderParams& old_params) {
	ID id = world.make_ID();
	Entity* entity = world.make<Entity>(id);
	Transform* trans = world.make<Transform>(id);
	Camera* cam = world.make<Camera>(id);

	Transform trans_of_model;
	trans_of_model.rotation = asset.rot_preview.rot;

	glm::mat4 model_m = trans_of_model.compute_model_matrix();
	
	Model* model = RHI::model_manager.get(asset.handle);
	AABB aabb;
	for (Mesh& sub_mesh : model->meshes) {
		aabb.update_aabb(sub_mesh.aabb);
	}

	glm::mat4 apply_model_m = asset.trans.compute_model_matrix();
	//aabb = aabb.apply(apply_model_m);

	glm::vec3 center = (aabb.max + aabb.min) / 2.0f;
	float radius = 0;

	glm::vec4 verts[8];
	aabb_to_verts(&aabb, verts);
	for (int i = 0; i < 8; i++) {
		radius = glm::max(radius, glm::length(glm::vec3(verts[i]) - center));
	}
	
	double fov = glm::radians(60.0f);

	trans->position = center;
	trans->position.z += (radius) / glm::tan(fov / 2.0);

	CommandBuffer cmd_buffer;
	RenderParams params = create_preview_command_buffer(cmd_buffer, old_params, self, cam, world);

	model->render(0, &model_m, asset.materials, params);

render_preview_to_buffer(self, params, cmd_buffer, asset.rot_preview.preview, world);

world.free_now_by_id(id);
}

void render_preview_for(World& world, AssetTab& self, MaterialAsset& asset, RenderParams& old_params) {
	ID id = world.make_ID();
	Entity* entity = world.make<Entity>(id);
	Transform* trans = world.make<Transform>(id);
	trans->position.z = 2.5;
	Camera* cam = world.make<Camera>(id);

	Model* model = RHI::model_manager.get(load_Model("sphere.fbx"));

	Transform trans_of_sphere;
	trans_of_sphere.rotation = asset.rot_preview.rot;

	glm::mat4 model_m = trans_of_sphere.compute_model_matrix();

	vector<Handle<Material>> materials = { asset.handle };

	CommandBuffer cmd_buffer;
	RenderParams render_params = create_preview_command_buffer(cmd_buffer, old_params, self, cam, world);

	model->render(0, &model_m, materials, render_params);

	render_preview_to_buffer(self, render_params, cmd_buffer, asset.rot_preview.preview, world);

	world.free_now_by_id(id);
}

void rot_preview(RotatablePreview& self) {
	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Preview");

	ImGui::Image((ImTextureID)texture::id_of(self.preview), ImVec2(512, 512), ImVec2(0, 1), ImVec2(1, 0));
	
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
}

void edit_color(glm::vec4& color, StringView name, glm::vec2 size) {

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

void edit_color(glm::vec3& color, StringView name, glm::vec2 size) {
	glm::vec4 color4 = glm::vec4(color, 1.0f);
	edit_color(color4, name, size);
	color = color4;
}

void channel_image(Handle<Texture>& image, StringView name) {
	static Handle<Texture> tex = load_Texture("solid_white.png");
	
	if (image.id == INVALID_HANDLE) { ImGui::Image((ImTextureID)texture::id_of(tex), ImVec2(200, 200), ImVec2(0, 1), ImVec2(1, 0), ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); }
	else ImGui::Image((ImTextureID)texture::id_of(image), ImVec2(200, 200), ImVec2(0, 1), ImVec2(1, 0));
	
	if (ImGui::IsMouseDragging() && ImGui::IsItemHovered()) {
		image.id = INVALID_HANDLE;
	}

	accept_drop("DRAG_AND_DROP_IMAGE", &image, sizeof(Handle<Texture>));
	ImGui::SameLine();

	ImGui::Text(name.c_str());
	ImGui::SameLine(ImGui::GetWindowWidth() - 300);
}

void set_params_for_shader_graph(Material* mat);

void inspect_material_params(Editor& editor, Material* material) {
	for (auto& param : material->params) {
		DiffUtil diff_util(&param, &temporary_allocator);

		auto shader = RHI::shader_manager.get(material->shader);
		auto& uniform = shader->uniforms[param.loc.id];

		if (param.type == Param_Vec2) {
			ImGui::PushItemWidth(200.0f);
			ImGui::InputFloat2(uniform.name.c_str(), &param.vec2.x);
		}
		if (param.type == Param_Vec3) {
			edit_color(param.vec3, uniform.name);

			ImGui::SameLine();
			ImGui::Text(uniform.name.c_str());
		}
		if (param.type == Param_Image) {
			ImGui::Image((ImTextureID)texture::id_of(param.image), ImVec2(200, 200), ImVec2(0, 1), ImVec2(1, 0));

			accept_drop("DRAG_AND_DROP_IMAGE", &param.image, sizeof(Handle<Texture>));

			ImGui::SameLine();
			ImGui::Text(uniform.name.c_str());
		}
		if (param.type == Param_Int) {
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::InputInt(uniform.name.c_str(), &param.integer);
		}

		if (param.type == Param_Float) {
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::SliderFloat(uniform.name.c_str(), &param.real, -1.0f, 1.0f);
		}

		if (param.type == Param_Channel3) {
			channel_image(param.channel3.image, uniform.name);
			edit_color(param.channel3.color, uniform.name, glm::vec2(50, 50));
		}

		if (param.type == Param_Channel2) {
			channel_image(param.channel2.image, uniform.name);
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::InputFloat2(tformat("##", uniform.name).c_str(), &param.vec2.x);
		}

		if (param.type == Param_Channel1) {
			channel_image(param.channel1.image, uniform.name);
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.3f);
			ImGui::SliderFloat(tformat("##", uniform.name).c_str(), &param.channel1.value, 0, 1.0f);
		}

		diff_util.submit(editor, "Material property");
	}
}

void asset_properties(MaterialAsset* mat_asset, Editor& editor, World& world, AssetTab& self, RenderParams& params) {

	Material* material = RHI::material_manager.get(mat_asset->handle);
	void* data = material;

	reflect::TypeDescriptor_Struct* material_type = (reflect::TypeDescriptor_Struct*)reflect::TypeResolver<Material>::get();

	auto& name = mat_asset->name;

	render_name(name, self.default_font);

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Material");

	for (auto field : material_type->members) {
		if (field.name == "name");
		else if (field.name == "shader") {
			Shader* shad = RHI::shader_manager.get(material->shader);

			if (ImGui::Button(shad->f_filename.c_str())) {
				ImGui::OpenPopup("StandardShaders");
			}

			if (accept_drop("DRAG_AND_DROP_SHADER", &material->shader, sizeof(Handle<Shader>))) {
				set_params_for_shader_graph(material);
			}

			if (ImGui::BeginPopup("StandardShaders")) {
				if (ImGui::Button("shaders/pbr.frag")) {
					Handle<Shader> new_shad = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");
					if (new_shad.id != material->shader.id) {
						set_base_shader_params(material, new_shad);
						material->set_vec2("transformUVs", glm::vec2(1, 1));
					}
				}

				if (ImGui::Button("shaders/tree.frag")) {
					Handle<Shader> new_shad = load_Shader("shaders/tree.vert", "shaders/tree.frag");
					if (new_shad.id != material->shader.id) {
						set_base_shader_params(material, new_shad);
						material->set_float("cutoff", 0.1f),
						material->set_vec2("transformUVs", glm::vec2(1, 1));
					}
				}
				
				if (ImGui::Button("shaders/paralax_pbr.frag")) {
					Handle<Shader> new_shad = load_Shader("shaders/pbr.vert", "shaders/paralax_pbr.frag");

					if (new_shad.id != material->shader.id) {
						set_base_shader_params(material, new_shad);
						material->set_image("material.height", load_Texture("black.png"));
						material->set_image("material.ao", load_Texture("solid_white.png")),
						material->set_int("steps", 5);
						material->set_float("depth_scale", 1);
						material->set_vec2("transformUVs", glm::vec2(1, 1));
					}
				}


				ImGui::EndPopup();
			}

		}
		else if (field.name == "params") {
			inspect_material_params(editor, material);
		}
		else {
			render_fields(field.type, (char*)data + field.offset, field.name, world);
		}
	}

	rot_preview(mat_asset->rot_preview);

	render_preview_for(world, self, *mat_asset, params);
}

void asset_properties(ModelAsset* mod_asset, Editor& editor, World& world, AssetTab& self, RenderParams& params) {
	Model* model = RHI::model_manager.get(mod_asset->handle);

	render_name(mod_asset->name, self.default_font);

	DiffUtil diff_util(&mod_asset->trans, &temporary_allocator);

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Transform");
	ImGui::InputFloat3("position", &mod_asset->trans.position.x);
	get_on_inspect_gui("glm::quat")(&mod_asset->trans.rotation, "rotation", world);
	ImGui::InputFloat3("scale", &mod_asset->trans.scale.x);

	diff_util.submit(editor, "Properties Material");

	if (ImGui::Button("Apply")) {
		model->load_in_place(mod_asset->trans.compute_model_matrix());
		mod_asset->rot_preview.rot_deg = glm::vec2();
		mod_asset->rot_preview.current = glm::vec2();
		mod_asset->rot_preview.previous = glm::vec2();
		mod_asset->rot_preview.rot = glm::quat();
	}

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Materials");

	for (int i = 0; i < mod_asset->materials.length; i++) {
		StringBuffer prefix = model->materials[i] + " : ";
		
		get_on_inspect_gui("Material")(&mod_asset->materials[i], prefix, world);
	}

	diff_util.submit(editor, "Asset Properties");

	rot_preview(mod_asset->rot_preview);

	render_preview_for(world, self, *mod_asset, params);
}

StringBuffer name_from_filename(StringView filename) {
	int after_slash = filename.find_last_of('\\') + 1;
	return filename.sub(after_slash, filename.find_last_of('.'));
}

void add_asset(AssetTab& self, ID id) {
	self.assets.by_id<AssetFolder>(self.current_folder)->contents.append(id);
}

void import_model(World& world, Editor& editor, AssetTab& self, RenderParams& params, StringView filename) {	
	Handle<Model> handle = load_Model(filename, true);
	Model* model = RHI::model_manager.get(handle);

	ID id = self.assets.make_ID();
	ModelAsset* model_asset = self.assets.make<ModelAsset>(id);

	model_asset->handle = handle;

	insert_model_handle_to_asset(model_asset);

	for (StringView name : model->materials) {
		model_asset->materials.append(self.default_material);
	}

	model_asset->name = name_from_filename(filename);
	
	render_preview_for(world, self, *model_asset, params);

	add_asset(self, id);
}

void import_texture(Editor& editor, AssetTab& self, StringView filename) {
	Handle<Texture> handle = load_Texture(filename);

	ID id = self.assets.make_ID();
	TextureAsset* folder = self.assets.make<TextureAsset>(id);
	folder->handle = handle;
	folder->name = name_from_filename(filename);

	add_asset(self, id);
}

void import_shader(Editor& editor, AssetTab& self, StringView filename) {
	StringBuffer frag_filename(filename.sub(0, filename.length - strlen(".vert")));
	frag_filename += ".frag";

	Handle<Shader> handle = load_Shader(filename, frag_filename);
	ID id = self.assets.make_ID();
	ShaderAsset* shader_asset = self.assets.make<ShaderAsset>(id);

	add_asset(self, id);
}

void import_filename(Editor& editor, World& world, RenderParams& params, AssetTab& self, std::wstring& w_filename) {
	StringBuffer filename;
	size_t i;
	filename.reserve(w_filename.size());
	wcstombs_s(&i, filename.data, w_filename.size() + 1, w_filename.c_str(), w_filename.size());
	filename.length = w_filename.size();

	StringBuffer asset_path;
	if (!Level::to_asset_path(filename.c_str(), &asset_path)) {
		asset_path = StringView(filename).sub(filename.find_last_of('\\') + 1, filename.size());

		std::string new_filename = Level::asset_path(asset_path).c_str();
		std::wstring w_new_filename = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(new_filename);

		if (!CopyFile(w_filename.c_str(), w_new_filename.c_str(), true)) {
			log("Could not copy filename");
		}
	}

	log(asset_path);

	if (asset_path.ends_with(".fbx") || asset_path.ends_with(".FBX")) {
		import_model(world, editor, self, params, asset_path);
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

MaterialAsset* register_new_material(World& world, AssetTab& self, Editor& editor, RenderParams& params, ID id) {
	MaterialAsset* asset = self.assets.by_id<MaterialAsset>(id);

	render_preview_for(world, self, *asset, params);
	add_asset(self, id);

	insert_material_handle_to_asset(asset);
	return asset;
}

MaterialAsset* create_new_material(World& world, AssetTab& self, Editor& editor, RenderParams& params) {
	ID id = self.assets.make_ID();
	MaterialAsset* asset = self.assets.make<MaterialAsset>(id);
	asset->handle = create_default_material();
	asset->name = "Empty";

	return register_new_material(world, self, editor, params, id);
}

AssetTab::AssetTab() {
	{
		AttachmentSettings attachment(this->preview_map);

		FramebufferSettings settings;
		settings.width = 512;
		settings.height = 512;
		settings.color_attachments.append(attachment);

		this->preview_fbo = Framebuffer(settings);
	}

	{
		AttachmentSettings attachment(this->preview_tonemapped_map);

		FramebufferSettings settings;
		settings.width = 512;
		settings.height = 512;
		settings.color_attachments.append(attachment);

		this->preview_tonemapped_fbo = Framebuffer(settings);
	}

	assets.add(new Store<AssetFolder>(100));
	assets.add(new Store<ModelAsset>(100));
	assets.add(new Store<TextureAsset>(100));
	assets.add(new Store<MaterialAsset>(100));
	assets.add(new Store<ShaderAsset>(100));

	ID id = assets.make_ID();

	//AssetFolder* folder = assets.make<AssetFolder>(id);
	//folder->name = "Assets";
	
	this->toplevel = id;
	this->current_folder = id;
}

void AssetTab::update(World& world, Editor& editor, UpdateParams& params) {
}

void AssetTab::render(World& world, Editor& editor, RenderParams& params) {
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
			DefferedMoveFolder deffered_move_folder;

			int folder_tree = this->current_folder;
			vector<const char*> folder_file_path;
			vector<ID> folder_id;
			folder_file_path.allocator = &temporary_allocator;
			folder_id.allocator = &temporary_allocator;

			while (folder_tree >= 0) {
				AssetFolder* folder = assets.by_id<AssetFolder>(folder_tree);
				folder_tree = folder->owner;

				folder_file_path.append(folder->name.c_str());
				folder_id.append(assets.id_of(folder));
			}

			for (int i = folder_file_path.length - 1; i >= 0; i--) {
				if (ImGui::Button(folder_file_path[i])) {
					this->current_folder = folder_id[i];
				}

				deffered_move_folder.accept_drop(folder_id[i]);

				ImGui::SameLine();
				ImGui::Text("\\");
				ImGui::SameLine();
			}

			deffered_move_folder.apply(*this);
		}

		{
			ImGui::SameLine(ImGui::GetWindowWidth() - 400);
			ImGui::Text("Filter");
			ImGui::SameLine();
			ImGui::InputText("Filter", filter);
		}

		ImGui::Separator();

		render_assets(editor, *this, filter, &deselect);

		if (open_create_asset && deselect) ImGui::OpenPopup("CreateAsset");

		if (ImGui::BeginPopup("CreateAsset"))
		{
			if (ImGui::MenuItem("Import")) {
				std::wstring filename = open_dialog(editor);
				if (filename != L"") import_filename(editor, world, params, *this, filename);
			}

			if (ImGui::MenuItem("New Material"))
			{
				create_new_material(world, *this, editor, params);
			}

			if (ImGui::MenuItem("New Shader"))
			{
				create_new_shader(world, *this, editor, params);
			}

			if (ImGui::MenuItem("New Folder"))
			{
				ID id = assets.make_ID();
				AssetFolder* folder = assets.make<AssetFolder>(id);
				folder->name = "Empty Folder";
				folder->owner = this->current_folder;

				add_asset(*this, id);
			}

			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("EditAsset"))
		{
			if (ImGui::MenuItem("Delete")) { //todo right clicking will not select
				assets.free_now_by_id(selected);

				AssetFolder* folder = assets.by_id<AssetFolder>(current_folder);

				for (int i = 0; i < folder->contents.length; i++) {
					if (folder->contents[i] == selected) {
						folder->contents[i] = folder->contents[folder->contents.length - 1];
						folder->contents.pop();
						break;
					}
				}

				//editor.submit_action(new DestroyAction(assets, selected));
			}

			ImGui::EndPopup();
		}
	}

	ImGui::End();

	if (ImGui::Begin("Asset Settings")) {
		if (selected != -1) {
			auto tex = assets.by_id<TextureAsset>(selected);
			auto shad = assets.by_id<ShaderAsset>(selected);
			auto mod = assets.by_id<ModelAsset>(selected);
			auto mat = assets.by_id<MaterialAsset>(selected);

			if (tex) asset_properties(tex, editor, world, *this, params);
			if (shad) asset_properties(shad, editor, world, *this, params);
			if (mod) asset_properties(mod, editor, world, *this, params);
			if (mat) asset_properties(mat, editor, world, *this, params);
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

unsigned int read_save_file(StringView filename, DeserializerBuffer* serializer) {
	char* buffer;
	File file;
	file.open(filename, File::ReadFileB);
	unsigned int length = file.read_binary(&buffer);
	
	new (serializer) DeserializerBuffer(buffer, length);

	return serializer->read_int();
}

void AssetTab::on_load(World& world, RenderParams& params) {
	{
		DeserializerBuffer serializer;
		unsigned int num = read_save_file("data/AssetFolder.ne", &serializer);

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
		unsigned int num = read_save_file("data/TextureAsset.ne", &serializer);

		constexpr int num_threads = 9;

		std::mutex tasks_lock;
		vector<Texture*> tasks;
		
		std::mutex result_lock;
		vector<Image> loaded_image;
		vector<Texture*> loaded_image_tex;

		int num_tasks_left = 0;

		for (int i = 0; i < num; i++) {
			ID id = serializer.read_int();
			TextureAsset* tex_asset = assets.make<TextureAsset>(id);
			assets.skipped_ids.append(id);

			StringBuffer filename;
			serializer.read_string(filename);
			serializer.read(reflect::TypeResolver<TextureAsset>::get(), tex_asset);

			Texture tex;
			tex.filename = std::move(filename);


			RHI::texture_manager.make(tex_asset->handle, std::move(tex));

			if (RHI::texture_manager.get(tex_asset->handle)->filename.starts_with("sms")) {
				tex.texture_id = 0;
				continue;
			}

			tasks.append(RHI::texture_manager.get(tex_asset->handle));
			num_tasks_left++;
		}

		std::thread threads[num_threads];

		for (int i = 0; i < num_threads; i++) {
			threads[i] = std::thread([&, i]() {
				while (true) {
					tasks_lock.lock();
					if (tasks.length == 0) {
						tasks_lock.unlock();
						break;
					}

					Texture* tex = tasks.pop();
					tasks_lock.unlock();

					Image image = load_Image(tex->filename);

					result_lock.lock();
					loaded_image.append(std::move(image));
					loaded_image_tex.append(tex);
					num_tasks_left--;
					result_lock.unlock();
				}
			});
		}
		

		while (true) {
			result_lock.lock();
			if (loaded_image_tex.length == 0) {
				result_lock.unlock();
				continue;
			}

			Texture* tex = loaded_image_tex.pop();
			Image image = loaded_image.pop();
			result_lock.unlock();
			tex->submit(image);

			if (num_tasks_left == 0 && loaded_image_tex.length == 0) {
				break;
			}
		}
		
		for (int i = 0; i < num_threads; i++) {
			threads[i].join();
		}
	}

	{
		DeserializerBuffer serializer;
		unsigned int num = read_save_file("data/MaterialAsset.ne", &serializer);

		auto paralax = load_Shader("shaders/pbr.vert", "shaders/paralax_pbr.frag");
		auto pbr_shader = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");
		auto tree = load_Shader("shaders/tree.vert", "shaders/tree.frag");

		for (int i = 0; i < num; i++) {
			ID id = serializer.read_int();
			MaterialAsset* mat_asset = assets.make<MaterialAsset>(id);
			assets.skipped_ids.append(id);

			Material mat;

			StringBuffer v_filename, f_filename;
			serializer.read_string(v_filename);
			serializer.read_string(f_filename);

			mat.shader = load_Shader(v_filename, f_filename);

			unsigned int length = serializer.read_int();

			for (int i = 0; i < length; i++) {
				Param param;
				StringBuffer name;

				serializer.read_string(name);

				serializer.read(reflect::TypeResolver<Param>::get(), &param);

				param.loc = location(mat.shader, name);
				if (param.type == Param_Channel1) param.channel1.scalar_loc = location(mat.shader, format(name, "_scalar"));
				if (param.type == Param_Channel2) param.channel2.scalar_loc = location(mat.shader, format(name, "_scalar"));
				if (param.type == Param_Channel3) param.channel3.scalar_loc = location(mat.shader, format(name, "_scalar"));

				mat.params.append(param);
			}

			serializer.read(reflect::TypeResolver<MaterialAsset>::get(), mat_asset);

			insert_material_handle_to_asset(mat_asset);

			RHI::material_manager.make(mat_asset->handle, std::move(mat));

			if (i == 0) this->default_material = mat_asset->handle;

			render_preview_for(world, *this, *mat_asset, params);
		}
	}

	{
		DeserializerBuffer serializer;
		unsigned int num = read_save_file("data/ModelAsset.ne", &serializer);

		for (int i = 0; i < num; i++) {
			ID id = serializer.read_int();
			ModelAsset* mod_asset = assets.make<ModelAsset>(id);
			assets.skipped_ids.append(id);

			Model model;
			serializer.read_string(model.path);
			serializer.read(reflect::TypeResolver<ModelAsset>::get(), mod_asset);

			model.load_in_place(mod_asset->trans.compute_model_matrix());

			insert_model_handle_to_asset(mod_asset);

			RHI::model_manager.make(mod_asset->handle, std::move(model));

			render_preview_for(world, *this, *mod_asset, params);
		}
	}

	
	{
		DeserializerBuffer serializer;
		unsigned int num = read_save_file("data/ShaderAsset.ne", &serializer);

		for (int i = 0; i < num; i++) {
			ID id = serializer.read_int();
			ShaderAsset* shad_asset = assets.make<ShaderAsset>(id);
			assets.skipped_ids.append(id);

			deserialize_shader_asset(serializer, shad_asset);
			insert_shader_handle_to_asset(shad_asset);
			
			load_Shader_for_graph(shad_asset);
		}
	}

}

#define BEGIN_SAVE(type_name) { \
SerializerBuffer serializer; \
auto filtered = assets.filter<type_name>(); \
serializer.write_int(filtered.length); \
\
for (type_name* asset : filtered) { \
	\
		serializer.write_int(assets.id_of(asset));

#define END_SAVE(filename) } \
File file; \
file.open(filename, File::WriteFileB); \
file.write({ serializer.data, serializer.index }); \
}

void AssetTab::on_save() {
	BEGIN_SAVE(AssetFolder)
		serializer.write(reflect::TypeResolver<AssetFolder>::get(), asset);
	END_SAVE("data/AssetFolder.ne")
	
	BEGIN_SAVE(TextureAsset)
		Texture* tex = RHI::texture_manager.get(asset->handle);
		serializer.write_string(tex->filename);
		serializer.write(reflect::TypeResolver<TextureAsset>::get(), asset);
	END_SAVE("data/TextureAsset.ne")
	
	
	BEGIN_SAVE(MaterialAsset)
		Material* mat = RHI::material_manager.get(asset->handle);
		
		Shader* shader = RHI::shader_manager.get(mat->shader);

		serializer.write_string(shader->v_filename);
		serializer.write_string(shader->f_filename);

		serializer.write_int(mat->params.length);
	
		for (Param& param : mat->params) {
			serializer.write_string(shader->uniforms[param.loc.id].name);
			serializer.write(reflect::TypeResolver<Param>::get(), &param);
		}
	
		serializer.write(reflect::TypeResolver<MaterialAsset>::get(), asset);
	END_SAVE("data/MaterialAsset.ne")
	
	BEGIN_SAVE(ModelAsset)
		serializer.write_string(RHI::model_manager.get(asset->handle)->path);
		serializer.write(reflect::TypeResolver<ModelAsset>::get(), asset);
	END_SAVE("data/ModelAsset.ne")

	BEGIN_SAVE(ShaderAsset)
			serialize_shader_asset(serializer, asset);
	END_SAVE("data/ShaderAsset.ne")
}

#undef BEGIN_SAVE
#undef END_SAVE