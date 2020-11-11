#pragma once

#include "core/container/sstring.h"
#include "engine/handle.h"
#include "handle.h"
#include "preview.h"
#include <glm/vec3.hpp>

struct AssetInfo;
struct AssetPreviewResources;
struct Renderer;
struct Window;
struct AssetNode;
struct Material;

struct AssetExplorer {
	AssetInfo& info;
	AssetPreviewResources& preview_resources;

	sstring filter;

	asset_handle current_folder;
	asset_handle selected = { INVALID_HANDLE };

	struct ImFont* filename_font = NULL;
	struct ImFont* default_font = NULL;
};

struct AssetTab {
	Renderer& renderer;
	Window& window;

	AssetInfo& info;
	AssetExplorer explorer;

	AssetPreviewResources preview_resources;
    
    uint drop_file_callback;

	AssetTab(Renderer&, AssetInfo&, Window& window);

	void register_callbacks(struct Window&, struct Editor&);
    void remove_callbacks(struct Window&, struct Editor&);
	void render(struct World&, struct Editor&, struct RenderPass&);
};

//refl::Struct* get_AssetNode_type();

bool save_asset_info(AssetPreviewResources& resources, AssetInfo& info, struct SerializerBuffer&, const char** err);
bool load_asset_info(AssetPreviewResources& resources, AssetInfo& info, struct DeserializerBuffer&, const char** err);

material_handle make_new_material(struct AssetTab& self, struct Editor& editor);
model_handle import_model(AssetTab& self, string_view filename);

AssetNode* get_asset(AssetInfo& self, asset_handle handle);
asset_handle add_asset_to_current_folder(AssetTab& self, AssetNode&& node);

void render_name(sstring& name, struct ImFont* font = nullptr);
bool accept_drop(const char* drop_type, void* ptr, unsigned int size);

//void insert_shader_handle_to_asset(AssetTab& asset_tab, ShaderAsset* asset);
void inspect_material_params(Editor& editor, Material* material);
