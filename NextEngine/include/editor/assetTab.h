#pragma once

#include "core/vector.h"
#include "core/handle.h"
#include <string>

struct TextureAsset {
	Handle<struct Texture> handle;
};

struct ModelAsset {
	Handle<struct Model> handle = { INVALID_HANDLE };
	vector<struct Material> materials;
	Handle<struct Texture> preview = { INVALID_HANDLE };
};

struct ShaderAsset {
	Handle<struct Shader> handle = { INVALID_HANDLE };
};

struct AssetFolder {
	vector<TextureAsset> textures;
	vector<ModelAsset> models;
	vector<ShaderAsset> shaders;

	vector<AssetFolder> sub_folders;
};

struct AssetTab {
	AssetFolder assets;
	std::string filter;

	void register_callbacks(struct Window&, struct Editor&);
	void render(struct World&, struct Editor&, struct RenderParams&);
};