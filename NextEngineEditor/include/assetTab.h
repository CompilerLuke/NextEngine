#pragma once

#include "core/container/vector.h"
#include "core/handle.h"
#include "graphics/rhi/frame_buffer.h"
#include "ecs/ecs.h"
#include <glm/gtc/quaternion.hpp>
#include "components/transform.h"
#include "core/container/string_buffer.h"
#include "graphics/renderer/material_system.h"

struct ShaderGraph;
struct AssetManager;
struct ImFont;
struct RenderCtx;
struct Camera;
struct World;
struct CommandBuffer;
struct Material;
struct Renderer;
struct Window;

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

struct TextureAsset {
	texture_handle handle;
	string_buffer name;

	REFLECT(NO_ARG)
};

struct RotatablePreview {
	texture_handle preview = { INVALID_HANDLE };
	glm::vec2 current;
	glm::vec2 previous;
	glm::vec2 rot_deg;
	glm::quat rot;
};

struct ModelAsset {
	model_handle handle = { INVALID_HANDLE };
	vector<material_handle> materials;
	string_buffer name;
	
	Transform trans;

	RotatablePreview rot_preview;

	REFLECT(NO_ARG)
};

struct ShaderAsset {
	shader_handle handle = { INVALID_HANDLE };
	string_buffer name;

	vector<Param> shader_arguments;

	ShaderGraph* graph = NULL; //todo this leaks now

	REFLECT(NO_ARG)
};

struct MaterialAsset {
	material_handle handle = { INVALID_HANDLE };
	RotatablePreview rot_preview;
	string_buffer name;

	REFLECT()
};

struct AssetFolder {
	asset_folder_handle handle = { INVALID_HANDLE }; //contains id to itself, used to select folder to move
	
	string_buffer name;
	vector<uint> contents;
	int owner = -1;

	REFLECT(NO_ARG)
};

struct AssetTab {
	Renderer& renderer;
	AssetManager& asset_manager;
	Window& window;

	Framebuffer preview_fbo;
	Framebuffer preview_tonemapped_fbo;
	texture_handle preview_map;
	texture_handle preview_tonemapped_map;
	struct ImFont* filename_font = NULL;
	struct ImFont* default_font = NULL;

	vector<MaterialAsset*> material_handle_to_asset; 
	vector<ModelAsset*> model_handle_to_asset;
	vector<ShaderAsset*> shader_handle_to_asset;

	World assets;
	ID toplevel;
	ID current_folder;
	int selected = -1;

	material_handle default_material;
	
	string_buffer filter;

	AssetTab(Renderer&, AssetManager&, Window& window);

	void register_callbacks(struct Window&, struct Editor&);
	void render(struct World&, struct Editor&, struct RenderCtx&);

	void on_save();
	void on_load(struct World& world, struct RenderCtx& params);
};

MaterialAsset* register_new_material(World& world, AssetTab& self, Editor& editor, RenderCtx& params, ID mat_asset_handle);
MaterialAsset* create_new_material(struct World& world, struct AssetTab& self, struct Editor& editor, struct RenderCtx& params);

void add_asset(AssetTab& self, ID id);
void render_name(string_buffer& name, struct ImFont* font);

void edit_color(glm::vec3& color, string_view name, glm::vec2 size = glm::vec2(200, 200));
void edit_color(glm::vec4& color, string_view name, glm::vec2 size = glm::vec2(200, 200));

RenderCtx create_preview_command_buffer(CommandBuffer& cmd_buffer, RenderCtx& old_params, AssetTab& self, Camera* cam, World& world);
void render_preview_to_buffer(AssetTab& self, RenderCtx& params, CommandBuffer& cmd_buffer, texture_handle& preview, World& world);
void rot_preview(TextureManager& texture_manager, RotatablePreview& self);

bool accept_drop(const char* drop_type, void* ptr, unsigned int size);

void insert_shader_handle_to_asset(AssetTab& asset_tab, ShaderAsset* asset);
void inspect_material_params(Editor& editor, Material* material);