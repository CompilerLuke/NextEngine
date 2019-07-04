#pragma once

#include "core/vector.h"
#include "core/handle.h"
#include <string>
#include "graphics/frameBuffer.h"
#include "ecs/ecs.h"

struct TextureAsset {
	Handle<struct Texture> handle;
	std::string name;

	REFLECT()
};

struct ModelAsset {
	Handle<struct Model> handle = { INVALID_HANDLE };
	vector<Handle<struct Material>> materials;
	Handle<struct Texture> preview = { INVALID_HANDLE };
	std::string name;

	REFLECT()
};

struct ShaderAsset {
	Handle<struct Shader> handle = { INVALID_HANDLE };
	std::string name;

	REFLECT()
};

struct MaterialAsset {
	Handle<struct Material> handle = { INVALID_HANDLE };
	Handle<struct Texture> preview = { INVALID_HANDLE };
	std::string name;

	REFLECT()
};

struct AssetFolder {
	vector<unsigned int> contents;

	REFLECT()
};

struct AssetTab {
	Framebuffer preview_fbo;
	Framebuffer preview_tonemapped_fbo;
	Handle<struct Texture> preview_map;
	Handle<struct Texture> preview_tonemapped_map;

	World assets;
	ID toplevel;
	ID current_folder;
	int selected = -1;
	
	std::string filter;

	AssetTab();

	void register_callbacks(struct Window&, struct Editor&);
	void render(struct World&, struct Editor&, struct RenderParams&);
};