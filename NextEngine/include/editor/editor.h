#pragma once

#include "core/core.h"
#include <string>
#include "ecs/id.h"
#include "core/eventDispatcher.h"
#include "editor/assetTab.h"
#include "reflection/reflection.h"
#include "editor/lister.h"
#include "editor/displayComponents.h"
#include "ecs/ecs.h"
#include "editor/gizmo.h"
#include "editor/picking.h"
#include "graphics/renderPass.h"
#include "editor/diffUtil.h"
#include "graphics/frameBuffer.h"

struct DroppableField {
	void* ptr;
	StringBuffer typ;
	ID id;
};

struct SwitchCase {
	void* ptr;
	int tag;
	reflect::TypeDescriptor_Union* type;
	int case_of_union;
};

struct AddComponent {
	ID id;
	struct ComponentStore* store;
};

struct DeleteComponent {
	ID id;
	struct ComponentStore* store;
};

struct Icon {
	StringBuffer name;
	Handle<struct Texture> texture_id;
};

struct Editor {
	World world;
	Window& window;
	StringView game_code;

	Handle<Texture> scene_view;
	Framebuffer scene_view_fbo;

	PickingPass picking_pass;
	MainPass main_pass;

	int selected_id = -1;
	float editor_tab_width = 0.25f;
	float model_tab_width = 0.2f;
	bool playing_game = false;
	bool exit = false;

	Transform fly_trans;

	vector<Icon> icons;

	vector<std::unique_ptr<EditorAction>> undos;
	vector<std::unique_ptr<EditorAction>> redos;

	void submit_action(EditorAction*);

	ImTextureID get_icon(StringView name);
	
	AssetTab asset_tab;
	Lister lister;
	DisplayComponents display_components;
	Gizmo gizmo;

	bool droppable_field_active;
	DroppableField droppable_field;

	EventDispatcher<ID> selected;

	ENGINE_API Editor(const char* game_code, struct Window&);
	ENGINE_API ~Editor();

	glm::vec3 place_at_cursor(struct Input&);

	struct RenderParams* render_params_ptr;

	void update(struct UpdateParams&);
	void render(struct RenderParams&, struct Input&);
	void init_imgui();

	ENGINE_API void run();

	void select(int);
};

namespace ImGui {
	void InputText(const char*, StringBuffer&);
}

