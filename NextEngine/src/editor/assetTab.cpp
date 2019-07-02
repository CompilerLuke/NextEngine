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

#include <locale>
#include <codecvt>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <WinBase.h>

void AssetTab::register_callbacks(Window& window, Editor& editor) {

}

void render_assets(AssetFolder& folder, Editor& editor, World& world, const std::string& filter) {
	for (auto& tex : folder.textures) {

	}

	for (auto& shad : folder.shaders) {

	}

	for (auto& mod : folder.models) {
		Model* model = RHI::model_manager.get(mod.handle);
		ImGui::Button(model->path.c_str());
	}
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

Handle<Texture> render_preview_for(AssetTab& self, Handle<Model> handle) {
	return { INVALID_HANDLE };
}

void import_model(Editor& editor, AssetTab& self, std::string& filename) {	
	Handle<Model> handle = load_Model(filename);

	self.assets.models.append({
		handle,
		{},
		render_preview_for(self, handle)
	});
}

void import_texture(Editor& editor, AssetTab& self, std::string& filename) {

}

void import_shader(Editor& editor, AssetTab& self, std::string& filename) {

}

void import_filename(Editor& editor, AssetTab& self, std::wstring& w_filename) {
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
		import_model(editor, self, asset_path);
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

void AssetTab::render(World& world, Editor& editor, RenderParams& params) {
	if (ImGui::Begin("Assets")) {

		if (ImGui::Button("Import")) {
			std::wstring filename = open_dialog(editor);
			if (filename != L"") 
				import_filename(editor, *this, filename);
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