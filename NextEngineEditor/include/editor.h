#pragma once

#include "core/core.h"
#include <string>
#include "ecs/id.h"
#include "core/container/event_dispatcher.h"
#include "assetTab.h"
#include "core/reflection.h"
#include "lister.h"
#include "displayComponents.h"
#include "ecs/ecs.h"
#include "gizmo.h"
#include "picking.h"
#include "graphics/pass/render_pass.h"
#include "diffUtil.h"
#include "graphics/rhi/frame_buffer.h"
#include "shaderGraph.h"
#include "visualize_profiler.h"
#include "engine/application.h"
#include "components/flyover.h"
#include "graphics/assets/shader.h"

struct DroppableField {
	void* ptr;
	string_buffer typ;
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
	string_buffer name;
	texture_handle texture_id;
};

//todo split up into multiple classes
struct Editor {
	struct Engine& engine;

	struct World& copy_of_world;

	texture_handle scene_view;
	Framebuffer scene_view_fbo;

	int selected_id = -1;

	float viewport_width;
	float viewport_height;

	bool playing_game = false;
	bool game_fullscreen = false;
	bool exit = false;

	Application game;

	Transform fly_trans;

	vector<Icon> icons;

	vector<std::unique_ptr<EditorAction>> undos;
	vector<std::unique_ptr<EditorAction>> redos;

	void submit_action(EditorAction*);

	uint64_t get_icon(string_view name);
	
	FlyOverSystem fly_over_system;
	VisualizeProfiler profiler;
	ShaderEditor shader_editor;
	AssetTab asset_tab;
	Lister lister;
	DisplayComponents display_components;
	Gizmo gizmo;
	PickingSystem picking;
	OutlineSelected outline_selected;
	
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
	void InputText(const char*, string_buffer&);
}

