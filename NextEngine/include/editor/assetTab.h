#pragma once

#include "core/vector.h"
#include "core/handle.h"
#include <string>
#include "graphics/frameBuffer.h"
#include "ecs/ecs.h"
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
#include "components/transform.h"
#include "core/string_buffer.h"

struct TextureAsset {
	Handle<struct Texture> handle;
	StringBuffer name;

	REFLECT()
};

struct RotatablePreview {
	Handle<struct Texture> preview = { INVALID_HANDLE };
	glm::vec2 current;
	glm::vec2 previous;
	glm::vec2 rot_deg;
	glm::quat rot;
};

struct ModelAsset {
	Handle<struct Model> handle = { INVALID_HANDLE };
	vector<Handle<struct Material>> materials;
	StringBuffer name;
	
	Transform trans;

	RotatablePreview rot_preview;

	REFLECT()
};

struct ShaderAsset {
	Handle<struct Shader> handle = { INVALID_HANDLE };
	StringBuffer name;

	REFLECT()
};

struct MaterialAsset {
	Handle<struct Material> handle = { INVALID_HANDLE };
	RotatablePreview rot_preview;
	StringBuffer name;

	REFLECT()
};

struct AssetFolder {
	Handle<AssetFolder> handle = { INVALID_HANDLE }; //contains id to itself, used to select folder to move
	
	StringBuffer name;
	vector<unsigned int> contents;
	int owner = -1;

	REFLECT()
};

struct AssetTab {
	Framebuffer preview_fbo;
	Framebuffer preview_tonemapped_fbo;
	Handle<struct Texture> preview_map;
	Handle<struct Texture> preview_tonemapped_map;
	struct ImFont* filename_font = NULL;
	struct ImFont* default_font = NULL;

	static vector<MaterialAsset*> material_handle_to_asset; 
	static void insert_material_asset(MaterialAsset*);

	World assets;
	ID toplevel;
	ID current_folder;
	int selected = -1;

	Handle<Material> default_material;
	
	StringBuffer filter;

	AssetTab();

	void register_callbacks(struct Window&, struct Editor&);
	void render(struct World&, struct Editor&, struct RenderParams&);
	void update(struct World&, struct Editor&, struct UpdateParams&);

	void on_save();
	void on_load(struct World& world, struct RenderParams& params);
};

MaterialAsset* create_new_material(struct World& world, struct AssetTab& self, struct Editor& editor, struct RenderParams& params);