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

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <WinBase.h>

#include <iostream>

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

void render_assets(AssetFolder& folder, Editor& editor, World& world, const std::string& filter) {
	ImGui::Columns(10);
	for (auto& tex : folder.textures) {
		Texture* texture = RHI::texture_manager.get(tex.handle);

		render_name(tex.name);
		ImGui::Image((ImTextureID)texture->texture_id, ImVec2(256, 256));
		ImGui::NextColumn();
	}

	for (auto& shad : folder.shaders) {
		render_name(shad.name);
		ImGui::NextColumn();
	}

	for (auto& mod : folder.models) {
		Model* model = RHI::model_manager.get(mod.handle);
		Texture* tex = RHI::texture_manager.get(mod.preview);

		render_name(mod.name);
		ImGui::Image((ImTextureID)tex->texture_id, ImVec2(256, 256));
		ImGui::NextColumn();
	}

	ImGui::Columns(1);
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

std::string name_from_filename(std::string& filename) {
	return filename.substr(filename.find_last_of("\\") + 1, filename.size() - filename.find_last_of("."));
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

	ModelAsset model_asset;
	model_asset.handle = handle;
	model_asset.materials = std::move(materials);
	model_asset.name = name_from_filename(filename);
	
	render_preview_for(world, self, model_asset, params);

	self.assets.models.append(model_asset);
}

void import_texture(Editor& editor, AssetTab& self, std::string& filename) {
	Handle<Texture> handle = load_Texture(filename);

	self.assets.textures.append({
		handle,
		name_from_filename(filename)
	});
}

void import_shader(Editor& editor, AssetTab& self, std::string& filename) {
	std::string frag_filename = filename.substr(0, filename.find(".vert"));
	frag_filename += ".frag";

	Handle<Shader> handle = load_Shader(filename, frag_filename);
	
	self.assets.shaders.append({
		handle,
		name_from_filename(filename)
	});
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

AssetTab::AssetTab() {
	{
		AttachmentSettings attachment(this->preview_map);

		FramebufferSettings settings;
		settings.width = 256;
		settings.height = 256;
		settings.color_attachments.append(attachment);

		this->preview_fbo = Framebuffer(settings);
	}

	{
		AttachmentSettings attachment(this->preview_tonemapped_map);

		FramebufferSettings settings;
		settings.width = 256;
		settings.height = 256;
		settings.color_attachments.append(attachment);

		this->preview_tonemapped_fbo = Framebuffer(settings);
	}
	
}

void AssetTab::render(World& world, Editor& editor, RenderParams& params) {
	if (ImGui::Begin("Assets")) {

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

		render_assets(assets, editor, world, filter);
	}

	ImGui::End();
}