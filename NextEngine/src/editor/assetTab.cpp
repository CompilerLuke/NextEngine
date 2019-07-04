#include "stdafx.h"
#include "editor/assetTab.h"
#include "editor/editor.h"
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
#include <algorithm>
#include <locale>
#include <codecvt>
#include "graphics/primitives.h"
#include "graphics/materialSystem.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <WinBase.h>

#include <iostream>

REFLECT_STRUCT_BEGIN(AssetFolder)
REFLECT_STRUCT_MEMBER(contents)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(TextureAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(ShaderAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(ModelAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(MaterialAsset)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_END()

DEFINE_COMPONENT_ID(AssetFolder, 0)
DEFINE_COMPONENT_ID(TextureAsset, 1)
DEFINE_COMPONENT_ID(ShaderAsset, 2)
DEFINE_COMPONENT_ID(ModelAsset, 3)
DEFINE_COMPONENT_ID(MaterialAsset, 4)

void AssetTab::register_callbacks(Window& window, Editor& editor) {
	
}

void render_name(std::string& name) {
	char buff[50];
	memcpy(buff, name.c_str(), name.size() + 1);

	ImGui::PushItemWidth(-1);
	ImGui::PushID((void*)&name);
	ImGui::InputText("##", buff, 50);
	ImGui::PopItemWidth();
	name = buff;
	ImGui::PopID();

}

void render_asset(ImTextureID texture_id, std::string& name, AssetTab& tab, ID id) {
	bool selected = id == tab.selected;
	if (selected) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 5);

	if (ImGui::ImageButton((ImTextureID)texture_id, ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0))) {
		tab.selected = id;
	}

	if (selected) ImGui::PopStyleVar(1);

	render_name(name);
	ImGui::NextColumn();
}

void render_assets(ID folder_handle, Editor& editor, AssetTab& asset_tab, const std::string& filter) {
	World& world = asset_tab.assets;
	auto folder = world.by_id<AssetFolder>(folder_handle);
	
	ImGui::PushStyleColor(ImGuiCol_Column, ImVec4(0.16, 0.16, 0.16, 1));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16, 0.16, 0.16, 1));

	ImGui::Columns(16, 0, false);

	for (auto handle : folder->contents) {
		auto tex = world.by_id<TextureAsset>(handle);
		auto shad = world.by_id<ShaderAsset>(handle);
		auto mod = world.by_id<ModelAsset>(handle);
		auto mat = world.by_id<MaterialAsset>(handle);

		if (tex) {
			Texture* texture = RHI::texture_manager.get(tex->handle);
			render_asset((ImTextureID)texture->texture_id, tex->name, asset_tab, handle);
		}

		if (shad) {
			render_asset(editor.get_icon("shader"), shad->name, asset_tab, handle);
		}

		if (mod) {
			Texture* tex = RHI::texture_manager.get(mod->preview);
			render_asset((ImTextureID)tex->texture_id, mod->name, asset_tab, handle);
		}

		if (mat) {
			Texture* tex = RHI::texture_manager.get(mat->preview);
			render_asset((ImTextureID)tex->texture_id, mat->name, asset_tab, handle);
		}
	}

	ImGui::Columns(1);
	ImGui::PopStyleColor(2);
}

void asset_properties(TextureAsset* tex, Editor& editor, World& world, AssetTab& self, RenderParams& params) {

}

//Materials

void asset_properties(ShaderAsset* tex, Editor& editor, World& world, AssetTab& self, RenderParams& params) {

}

void asset_properties(MaterialAsset* mat_asset, Editor& editor, World& world, AssetTab& self, RenderParams& params) {
	Material* material = RHI::material_manager.get(mat_asset->handle);
	void* data = material;

	reflect::TypeDescriptor_Struct* material_type = (reflect::TypeDescriptor_Struct*)reflect::TypeResolver<Material>::get();

	auto& name = material->name;

	render_name(name);

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Material");

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
					ImGui::Image((ImTextureID)tex->texture_id, ImVec2(200, 200)); //todo refactor get_id into function
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

	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Preview");

	Texture* tex = RHI::texture_manager.get(mat_asset->preview);
	ImGui::Image((ImTextureID)tex->texture_id, ImVec2(512, 512));

}

void asset_properties(ModelAsset* tex, Editor& editor, World& world, AssetTab& self, RenderParams& params) {

}

wchar_t* to_wide_char(const char* orig);

bool endsWith(const std::string& str, const std::string& suffix)
{
	return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

bool startsWith(const std::string& str, const std::string& prefix)
{
	return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

std::wstring open_dialog(Editor& editor) {
	wchar_t filename[MAX_PATH];

	OPENFILENAME ofn;
	memset(&filename, 0, sizeof(filename));
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);

	ofn.hwndOwner = glfwGetWin32Window(editor.window.window_ptr);
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

void render_preview_for(World& world, AssetTab& self, ModelAsset& asset, RenderParams& old_params) {
	self.preview_fbo.bind();
	self.preview_fbo.clear_color(glm::vec4(0,0,0,1));
	self.preview_fbo.clear_depth(glm::vec4(0,0,0,1));

	Handle<Shader> tone_map = load_Shader("shaders/screenspace.vert", "shaders/preview.frag");

	ID id = world.make_ID();
	Entity* entity = world.make<Entity>(id);
	Transform* trans = world.make<Transform>(id);
	Camera* cam = world.make<Camera>(id);
	
	Model* model = RHI::model_manager.get(asset.handle);
	AABB aabb;
	for (Mesh& sub_mesh : model->meshes) {
		aabb.update_aabb(sub_mesh.aabb);
	}

	float max_z = std::max(aabb.max.y, aabb.max.z * 1.2f);
	
	//trans->position.z = 3;

	trans->position.z = max_z * 1.2;
	trans->position.y = aabb.min.y * 0.3 + aabb.max.y * 0.7;
	//trans->rotation = glm::quat(glm::radians(180.0f), glm::vec3(0, 0, 1));

	CommandBuffer cmd_buffer;
	RenderParams render_params = old_params;
	render_params.width = self.preview_fbo.width;
	render_params.height = self.preview_fbo.height;
	render_params.command_buffer = &cmd_buffer;

	cam->fov = 60;
	cam->update_matrices(world, render_params);

	glm::mat4 identity(1.0);
	model->render(0, &identity, asset.materials, render_params);

	cmd_buffer.submit_to_gpu(world, render_params);

	self.preview_fbo.unbind();
	self.preview_tonemapped_fbo.bind();
	self.preview_tonemapped_fbo.clear_color(glm::vec4(0,0,0,1));
	self.preview_tonemapped_fbo.clear_depth(glm::vec4(0, 0, 0, 1));

	shader::bind(tone_map);
	shader::set_int(tone_map, "frameMap", 0);
	shader::set_mat4(tone_map, "model", identity);
	texture::bind_to(self.preview_map, 0);

	render_quad();

	unsigned int texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, self.preview_fbo.width, self.preview_fbo.height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, self.preview_fbo.width, self.preview_fbo.height);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);

	Texture tex;
	tex.filename = "asset preview";
	tex.texture_id = texture_id;

	log("created asset");

	world.free_by_id(id);

	self.preview_tonemapped_fbo.unbind();

	asset.preview = RHI::texture_manager.make(std::move(tex));
}

void render_preview_for(World& world, AssetTab& self, MaterialAsset& asset, RenderParams& old_params) {
	self.preview_fbo.bind();
	self.preview_fbo.clear_color(glm::vec4(0, 0, 0, 1));
	self.preview_fbo.clear_depth(glm::vec4(0, 0, 0, 1));

	Handle<Shader> tone_map = load_Shader("shaders/screenspace.vert", "shaders/preview.frag");

	ID id = world.make_ID();
	Entity* entity = world.make<Entity>(id);
	Transform* trans = world.make<Transform>(id);
	Camera* cam = world.make<Camera>(id);

	Handle<Model> handle_sphere = load_Model("sphere.fbx");

	Model* model = RHI::model_manager.get(handle_sphere);

	trans->position.z = 2.5;

	CommandBuffer cmd_buffer;
	RenderParams render_params = old_params;
	render_params.width = self.preview_fbo.width;
	render_params.height = self.preview_fbo.height;
	render_params.command_buffer = &cmd_buffer;

	cam->fov = 60;
	cam->update_matrices(world, render_params);

	vector<Handle<Material>> materials = { asset.handle };

	glm::mat4 identity(1.0);
	model->render(0, &identity, materials, render_params);

	cmd_buffer.submit_to_gpu(world, render_params);

	self.preview_fbo.unbind();
	self.preview_tonemapped_fbo.bind();
	self.preview_tonemapped_fbo.clear_color(glm::vec4(0, 0, 0, 1));
	self.preview_tonemapped_fbo.clear_depth(glm::vec4(0, 0, 0, 1));

	shader::bind(tone_map);
	shader::set_int(tone_map, "frameMap", 0);
	shader::set_mat4(tone_map, "model", identity);
	texture::bind_to(self.preview_map, 0);

	render_quad();

	unsigned int texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, self.preview_fbo.width, self.preview_fbo.height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, self.preview_fbo.width, self.preview_fbo.height);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);

	Texture tex;
	tex.filename = "asset preview";
	tex.texture_id = texture_id;

	log("created asset");

	world.free_by_id(id);

	self.preview_tonemapped_fbo.unbind();

	asset.preview = RHI::texture_manager.make(std::move(tex));
}

std::string name_from_filename(std::string& filename) {
	int after_slash = filename.find_last_of("\\") + 1;
	return filename.substr(after_slash, filename.find_last_of(".") - after_slash);
}

void add_asset(AssetTab& self, ID id) {
	self.assets.by_id<AssetFolder>(self.current_folder)->contents.append(id);
}

void import_model(World& world, Editor& editor, AssetTab& self, RenderParams& params, std::string& filename) {	
	Handle<Model> handle = load_Model(filename);
	Model* model = RHI::model_manager.get(handle);
	Handle<Shader> shader = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");

	auto default_params = make_SubstanceMaterial("wood_2", "Stylized_Wood");

	vector<Handle<Material>> materials;
	for (std::string& name : model->materials) {
		Material mat = {
			name,
			shader,
			default_params,
			&default_draw_state
		};
		
		materials.append(RHI::material_manager.make(std::move(mat)));
	}

	ID id = self.assets.make_ID();
	ModelAsset* model_asset = self.assets.make<ModelAsset>(id);
	model_asset->handle = handle;
	model_asset->materials = std::move(materials);
	model_asset->name = name_from_filename(filename);
	
	render_preview_for(world, self, *model_asset, params);

	add_asset(self, id);
}

void import_texture(Editor& editor, AssetTab& self, std::string& filename) {
	Handle<Texture> handle = load_Texture(filename);

	ID id = self.assets.make_ID();
	TextureAsset* folder = self.assets.make<TextureAsset>(id);
	folder->handle = handle;
	folder->name = name_from_filename(filename);

	add_asset(self, id);
}

void import_shader(Editor& editor, AssetTab& self, std::string& filename) {
	std::string frag_filename = filename.substr(0, filename.find(".vert"));
	frag_filename += ".frag";

	Handle<Shader> handle = load_Shader(filename, frag_filename);
	ID id = self.assets.make_ID();
	ShaderAsset* shader_asset = self.assets.make<ShaderAsset>(id);

	add_asset(self, id);
}

void import_filename(Editor& editor, World& world, RenderParams& params, AssetTab& self, std::wstring& w_filename) {
	std::string filename = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(w_filename);
		
	std::string asset_path;
	if (!Level::to_asset_path(filename, &asset_path)) {
		asset_path = filename.substr(filename.find_last_of("\\") + 1, filename.size());

		std::string new_filename = Level::asset_path(asset_path);
		std::wstring w_new_filename = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(new_filename);

		if (!CopyFile(w_filename.c_str(), w_new_filename.c_str(), true)) {
			log("Could not copy filename");
		}
	}

	log(asset_path);

	if (endsWith(asset_path, ".fbx")) {
		import_model(world, editor, self, params, asset_path);
	}
	else if (endsWith(asset_path, ".jpg") || endsWith(asset_path, ".png")) {
		import_texture(editor, self, asset_path);
	}
	else if (endsWith(asset_path, ".vert") || endsWith(asset_path, ".frag")) {
		import_shader(editor, self, asset_path);
	}
	else {
		log("Unsupported extension for", asset_path);
	}
}

void create_new_material(World& world, AssetTab& self, Editor& editor, RenderParams& params) {
	Handle<Shader> pbr = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");
	
	Handle<Material> mat_handle = RHI::material_manager.make({
		"Empty",
		pbr,
		{
			make_Param_Image(location(pbr, "material.diffuse"), load_Texture("solid_white.png")),
			make_Param_Image(location(pbr, "material.roughness"), load_Texture("solid_white.png")),
			make_Param_Image(location(pbr, "material.metallic"), load_Texture("solid_white.png")),
			make_Param_Image(location(pbr, "material.normal"), load_Texture("normal.jpg")),
			make_Param_Vec2(location(pbr, "transformUVs"), glm::vec2(1, 1))
		},
		&default_draw_state
	});

	ID id = self.assets.make_ID();
	MaterialAsset* asset = self.assets.make<MaterialAsset>(id);
	asset->handle = mat_handle;
	asset->name = "Empty";
	
	render_preview_for(world, self, *asset, params);
	add_asset(self, id);
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

	assets.make<AssetFolder>(id);
	this->toplevel = id;
	this->current_folder = id;
}

void AssetTab::render(World& world, Editor& editor, RenderParams& params) {
	if (ImGui::Begin("Assets")) {
		if (ImGui::BeginPopup("CreateAsset"))
		{
			if (ImGui::MenuItem("New Material"))
			{
				create_new_material(world, *this, editor, params);
			}

			ImGui::EndPopup();
		}

		if (ImGui::IsWindowHovered()) {
			if (ImGui::GetIO().MouseClicked[0]) selected = -1;
			if (ImGui::GetIO().MouseClicked[1]) ImGui::OpenPopup("CreateAsset");
		}

		if (ImGui::Button("Import")) {
			std::wstring filename = open_dialog(editor);
			if (filename != L"") 
				import_filename(editor, world, params, *this, filename);
		}

		{
			char buff[50];
			memcpy(buff, filter.c_str(), filter.size() + 1);
			ImGui::SameLine();
			ImGui::InputText("Filter", buff, 50);
			filter = std::string(buff);
		}

		ImGui::Separator();

		render_assets(toplevel, editor, *this, filter);
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