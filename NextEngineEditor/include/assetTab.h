#pragma once

#include "core/container/vector.h"
#include "core/container/tvector.h"
#include "core/handle.h"
#include "graphics/rhi/frame_buffer.h"
#include "ecs/ecs.h"
#include <glm/gtc/quaternion.hpp>
#include "components/transform.h"
#include "core/container/string_buffer.h"
#include "graphics/assets/material.h"

struct ShaderGraph;
struct Assets;
struct ImFont;
struct RenderPass;
struct Camera;
struct World;
struct CommandBuffer;
struct Material;
struct Renderer;
struct Window;

/*
struct asset_folder_handle {
	uint id = INVALID_HANDLE;
};

struct texture_asset_handle {
	uint id = INVALID_HANDLE;
};

struct model_asset_handle {
	uint id = INVALID_HANDLE;
};

struct shader_asset_handle {
	uint id = INVALID_HANDLE;
};

struct material_asset_handle {
	uint id = INVALID_HANDLE;
};
*/

struct NamedAsset {
	uint handle;
	sstring name;
};

struct TextureAsset {
	texture_handle handle;
	sstring name;

	REFLECT(NO_ARG)
};

struct RotatablePreview {
	glm::vec2 preview_tex_coord;
	glm::vec2 current;
	glm::vec2 previous;
	glm::vec2 rot_deg;
	glm::quat rot;
};

struct ModelAsset {
	model_handle handle = { INVALID_HANDLE };
	sstring name;
	RotatablePreview rot_preview;
	
	vector<material_handle> materials;
	
	Transform trans;

	REFLECT(NO_ARG)
};

struct ShaderAsset {
	shader_handle handle = { INVALID_HANDLE };
	sstring name;

	vector<ParamDesc> shader_arguments;

	ShaderGraph* graph = NULL; //todo this leaks now

	REFLECT(NO_ARG)
};

struct MaterialAsset {
	material_handle handle = { INVALID_HANDLE };
	sstring name;
	RotatablePreview rot_preview;

	REFLECT()
};

struct asset_handle {
	uint id;

	REFLECT(NO_ARG)
};

struct AssetFolder {
	//asset_folder_handle handle = { INVALID_HANDLE }; //contains id to itself, used to select folder to move
	
	sstring name;
	vector<struct AssetNode> contents;
	asset_handle owner;

	REFLECT(NO_ARG)
};

struct AssetNode {
	enum Type { Texture, Material, Shader, Model, Folder, Count } type;
	asset_handle handle;

	union {
		NamedAsset asset;
		TextureAsset texture;
		MaterialAsset material;
		ShaderAsset shader;
		ModelAsset model;
		AssetFolder folder;
	};

	AssetNode();
	~AssetNode();
	AssetNode(AssetNode::Type type);
	void operator=(AssetNode&&);

	REFLECT(NO_ARG)
};

//todo HandleManager is a relatively slow
//and fragmented data structure, could get much better
//cache locality by copying and adjusting pointers

const uint MAX_ASSETS = 1000;
const uint MAX_PER_ASSET_TYPE = 200;

struct AssetPreviewResources {
	pipeline_layout_handle pipeline_layout;
	descriptor_set_handle pass_descriptor;
	descriptor_set_handle pbr_descriptor;
	UBOBuffer pass_ubo;
	Framebuffer preview_fbo;
	Framebuffer preview_tonemapped_fbo;
	texture_handle preview_map;
	texture_handle preview_tonemapped_map;
	texture_handle preview_atlas;
	bool atlas_layout_undefined = true;
	array<MAX_ASSETS, glm::vec2> free_atlas_slot;
};

struct AssetInfo {
	uint id_counter = 0;
	AssetNode* asset_handle_to_node[MAX_ASSETS];
	AssetNode* asset_type_handle_to_node[AssetNode::Count][MAX_ASSETS];
	AssetNode toplevel;
};

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

	vector<asset_handle> render_preview_for;
	material_handle default_material;

	AssetTab(Renderer&, AssetInfo&, Window& window);

	//void register_callbacks(struct Window&, struct Editor&);
	void render(struct World&, struct Editor&, struct RenderPass&);

	void on_save();
	void on_load(struct World& world);
};

const char* drop_types[AssetNode::Count] = {
	"DRAG_AND_DROP_TEXTURE",
	"DRAG_AND_DROP_MATERIAL",
	"DRAG_AND_DROP_SHADER",
	"DRAG_AND_DROP_MODEL",
	"DRAG_AND_DROP_FOLDER",
};

const RenderPass::ID PreviewPass = (RenderPass::ID)(RenderPass::PassCount + 0);
const RenderPass::ID PreviewPassTonemap = (RenderPass::ID)(RenderPass::PassCount + 1);

material_handle create_new_material(struct AssetTab& self, struct Editor& editor);
model_handle import_model(AssetTab& self, string_view filename);

AssetNode* get_asset(AssetInfo& self, asset_handle handle);
asset_handle add_asset_to_current_folder(AssetTab& self, AssetNode&& node);

void render_name(sstring& name, struct ImFont* font = nullptr);

void edit_color(glm::vec3& color, string_view name, glm::vec2 size = glm::vec2(200, 200));
void edit_color(glm::vec4& color, string_view name, glm::vec2 size = glm::vec2(200, 200));

RenderPass create_preview_command_buffer(CommandBuffer& cmd_buffer, AssetTab& self, Camera* cam, World& world);
void render_preview_to_buffer(AssetTab& self, RenderPass& params, CommandBuffer& cmd_buffer, texture_handle& preview, World& world);
void rot_preview(AssetPreviewResources& resources, RotatablePreview& self);
void preview_from_atlas(AssetPreviewResources& resources, RotatablePreview& preview, texture_handle* handle, glm::vec2* uv0, glm::vec2* uv1);
void preview_image(AssetPreviewResources& resources, RotatablePreview& preview, glm::vec2 size);

bool accept_drop(const char* drop_type, void* ptr, unsigned int size);

//void insert_shader_handle_to_asset(AssetTab& asset_tab, ShaderAsset* asset);
void inspect_material_params(Editor& editor, Material* material);