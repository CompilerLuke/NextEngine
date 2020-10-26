#pragma once

#include "core/core.h"
#include <string>
#include "ecs/id.h"
#include "core/container/event_dispatcher.h"
#include "assets/explorer.h"
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
#include "editor_viewport.h"
#include "engine/application.h"
#include "components/flyover.h"
#include "graphics/assets/shader.h"

struct World;
struct Time;
struct Input;
struct Assets;

/*
struct DroppableField {
	void* ptr;
	string_buffer typ;
	ID id;
};


struct SwitchCase {
	void* ptr;
	int tag;
	refl::TypeDescriptor_Union* type;
	int case_of_union;
};

struct AddComponent {
	ID id;
	struct ComponentStore* store;
};

struct DeleteComponent {
	ID id;
	struct ComponentStore* store;
};*/

struct Icon {
	string_buffer name;
	texture_handle texture_id;
};

//todo split up into multiple classes
struct Editor {
	Window& window;
	Renderer& renderer;
	World& world;
	Input& input;
	Time& time;

	struct World& copy_of_world;

	texture_handle scene_view;
	Framebuffer scene_view_fbo;

	int selected_id = -1;

	bool playing_game = false;
	bool game_fullscreen = false;
	bool exit = false;

	Application game;

	Transform fly_trans;

	vector<Icon> icons;

	EditorActions actions;

	texture_handle get_icon(string_view name);
	
	AssetInfo asset_info;

	VisualizeProfiler profiler;
	ShaderEditor shader_editor;
	AssetTab asset_tab;
	Lister lister;
	DisplayComponents display_components;
	Gizmo gizmo;
	PickingSystem picking;
	OutlineRenderState outline_selected;
	GizmoResources gizmo_resources;

	EditorViewport editor_viewport;
	
	//bool droppable_field_active;
	//DroppableField droppable_field;

	EventDispatcher<ID> selected;

	Editor(Modules& engine, const char* game_code);
	~Editor();

	glm::vec3 place_at_cursor();

	void init_imgui();

	void create_new_object(string_view, ID id);
	void begin_imgui(struct Input&);
	void end_imgui(struct CommandBuffer&);

	void select(int);
};

namespace ImGui {
	void Button(string_view);
	void InputText(const char*, sstring&);
	void InputText(const char*, string_buffer&);
}

