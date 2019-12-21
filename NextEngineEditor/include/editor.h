#pragma once

#include "core/core.h"
#include <string>
#include "ecs/id.h"
#include "core/eventDispatcher.h"
#include "assetTab.h"
#include "reflection/reflection.h"
#include "lister.h"
#include "displayComponents.h"
#include "ecs/ecs.h"
#include "gizmo.h"
#include "picking.h"
#include "graphics/renderPass.h"
#include "diffUtil.h"
#include "graphics/frameBuffer.h"
#include "shaderGraph.h"
#include "visualize_profiler.h"
#include "engine/application.h"

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
	class Engine& engine;

	Handle<Texture> scene_view;
	Framebuffer scene_view_fbo;

	PickingPass picking_pass;
	MainPass main_pass;

	int selected_id = -1;

	bool playing_game = false;
	bool exit = false;

	Application game;

	Transform fly_trans;

	vector<Icon> icons;

	vector<std::unique_ptr<EditorAction>> undos;
	vector<std::unique_ptr<EditorAction>> redos;

	void submit_action(EditorAction*);

	uint64_t get_icon(StringView name);
	
	VisualizeProfiler profiler;
	ShaderEditor shader_editor;
	AssetTab asset_tab;
	Lister lister;
	DisplayComponents display_components;
	Gizmo gizmo;
	
	bool droppable_field_active;
	DroppableField droppable_field;

	EventDispatcher<ID> selected;

	Editor(Engine& engine, const char* game_code);
	~Editor();

	glm::vec3 place_at_cursor();

	void init_imgui();

	void begin_imgui(struct Input&);
	void end_imgui();

	void select(int);
};

namespace ImGui {
	void InputText(const char*, StringBuffer&);
}

