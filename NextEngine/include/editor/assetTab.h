#pragma once

#include "core/vector.h"
#include "core/handle.h"
#include <string>
#include "graphics/frameBuffer.h"

struct TextureAsset {
	Handle<struct Texture> handle;
};

struct ModelAsset {
	Handle<struct Model> handle = { INVALID_HANDLE };
	vector<Handle<struct Material>> materials;
	Handle<struct Texture> preview = { INVALID_HANDLE };
};

struct ShaderAsset {
	Handle<struct Shader> handle = { INVALID_HANDLE };
};

struct MaterialAsset {
	Handle<struct Material> handle = { INVALID_HANDLE };
	Handle<struct Texture> preview = { INVALID_HANDLE };
};

struct AssetFolder {
	vector<TextureAsset> textures;
	vector<ModelAsset> models;
	vector<ShaderAsset> shaders;
	vector<MaterialAsset> materials;

	vector<AssetFolder> sub_folders;
};

struct AssetTab {
	Framebuffer preview_fbo;
	Framebuffer preview_tonemapped_fbo;
	Handle<struct Texture> preview_map;
	Handle<struct Texture> preview_tonemapped_map;

	AssetFolder assets;
	std::string filter;

	AssetTab();

	void register_callbacks(struct Window&, struct Editor&);
	void render(struct World&, struct Editor&, struct RenderParams&);
};