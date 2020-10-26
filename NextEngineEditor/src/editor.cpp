#include "editor.h"
#include "graphics/rhi/window.h"
#include "engine/input.h"
#include "graphics/renderer/renderer.h"
#include "core/io/logger.h"
#include "core/reflection.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_glfw.h>
#include "ecs/ecs.h"
#include "graphics/assets/shader.h"
#include "graphics/assets/model.h"
#include "graphics/assets/texture.h"
#include "core/time.h"
#include "graphics/pass/render_pass.h"
#include "graphics/renderer/ibl.h"
#include "components/lights.h"
#include "components/transform.h"
#include "components/camera.h"
#include "components/flyover.h"
#include "custom_inspect.h"
#include "picking.h"
#include "engine/vfs.h"
#include "graphics/assets/material.h"
#include "core/serializer.h"
#include "terrain.h"
#include <vendor/ImGuizmo/ImGuizmo.h>
#include "core/profiler.h"
#include "graphics/renderer/renderer.h"
#include "engine/engine.h"
#include "graphics/assets/assets.h"
#include <imgui/imgui_internal.h>


#include "generated.h"
#include <core/types.h>



#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


//theme by Derydoca 
void set_darcula_theme() {
	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;

	ImVec4 shade1 = ImColor(25, 25, 25);
	ImVec4 shade2 = ImColor(44, 44, 44);
	ImVec4 shade3 = ImColor(64, 64, 64);
	ImVec4 border = shade2; // ImColor(100, 100, 100);
	ImVec4 blue = ImColor(52, 159, 235);
	ImVec4 white = ImColor(255,255,255,255);
	ImVec4 none = ImColor(255, 255, 255, 0);

	colors[ImGuiCol_Text] = white;
	colors[ImGuiCol_TextDisabled] = ImVec4(0.500f, 0.500f, 0.500f, 1.000f);
	colors[ImGuiCol_WindowBg] = shade1;
	colors[ImGuiCol_ChildBg] = none;
	colors[ImGuiCol_PopupBg] = shade3;
	colors[ImGuiCol_Border] = border;
	colors[ImGuiCol_BorderShadow] = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
	colors[ImGuiCol_FrameBg] = shade2;
	colors[ImGuiCol_FrameBgHovered] = blue;
	colors[ImGuiCol_FrameBgActive] = blue;
	colors[ImGuiCol_TitleBg] = shade1;
	colors[ImGuiCol_TitleBgActive] = shade1;
	colors[ImGuiCol_TitleBgCollapsed] = shade1;
	colors[ImGuiCol_MenuBarBg] = shade1;
	colors[ImGuiCol_ScrollbarBg] = shade2;
	colors[ImGuiCol_ScrollbarGrab] = shade2;
	colors[ImGuiCol_ScrollbarGrabHovered] = blue;
	colors[ImGuiCol_ScrollbarGrabActive] = blue;
	colors[ImGuiCol_CheckMark] = blue;
	colors[ImGuiCol_SliderGrab] = blue;
	colors[ImGuiCol_SliderGrabActive] = blue;
	colors[ImGuiCol_Button] = shade2;
	colors[ImGuiCol_ButtonHovered] = blue;
	colors[ImGuiCol_ButtonActive] = shade2;
	colors[ImGuiCol_Header] = shade3; //ImVec4(0.25f, 0.25f, 0.25f, 1.000f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.000f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.3f, 0.3f, 0.3f, 1.000f);
	colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.391f, 0.391f, 0.391f, 1.000f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(1.000f, 1.000f, 1.000f, 0.250f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.000f, 1.000f, 1.000f, 0.670f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_Tab] = shade1;
	colors[ImGuiCol_TabHovered] = blue;
	colors[ImGuiCol_TabActive] = shade2;
	colors[ImGuiCol_TabUnfocused] = shade1;
	colors[ImGuiCol_TabUnfocusedActive] = shade1;
	colors[ImGuiCol_DockingPreview] = blue;
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.160f, 0.160f, 0.160f, 1.000f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.469f, 0.469f, 0.469f, 1.000f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.586f, 0.586f, 0.586f, 1.000f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(1.000f, 1.000f, 1.000f, 0.156f);
	colors[ImGuiCol_DragDropTarget] = blue;
	colors[ImGuiCol_NavHighlight] = ImVec4(0, 0, 0.000f, 1.000f);
	colors[ImGuiCol_NavWindowingHighlight] = blue;
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.586f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.586f);

	/*
	colors[ImGuiCol_Text] = ImVec4(0.9f, 0.9f, 0.9f, 1.000f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.500f, 0.500f, 0.500f, 1.000f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.160f, 0.160f, 1.000f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.280f, 0.280f, 0.280f, 0.000f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.313f, 0.313f, 0.313f, 1.000f);
	colors[ImGuiCol_Border] = ImVec4(0.28f, 0.28f, 0.28f, 1.000f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.28f, 0.28f, 0.28f, 1.000f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.200f, 0.200f, 0.200f, 1.000f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.280f, 0.280f, 0.280f, 1.000f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.000f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.000f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.148f, 0.148f, 0.148f, 1.000f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.000f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.160f, 0.160f, 0.160f, 1.000f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.277f, 0.277f, 0.277f, 1.000f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.300f, 0.300f, 0.300f, 1.000f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_CheckMark] = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.391f, 0.391f, 0.391f, 1.000f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_Button] = ImVec4(1.000f, 1.000f, 1.000f, 0.000f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(1.000f, 1.000f, 1.000f, 0.391f);
	colors[ImGuiCol_ButtonActive] = ImVec4(1.000f, 1.000f, 1.000f, 0.156f);
	colors[ImGuiCol_Header] = colors[ImGuiCol_TitleBg]; //ImVec4(0.25f, 0.25f, 0.25f, 1.000f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.000f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.3f, 0.3f, 0.3f, 1.000f);
	colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.391f, 0.391f, 0.391f, 1.000f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(1.000f, 1.000f, 1.000f, 0.250f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.000f, 1.000f, 1.000f, 0.670f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.20f, 0.20f, 1.000f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.352f, 0.352f, 0.352f, 1.000f);
	colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.000f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.20f, 0.20f, 0.20f, 1.000f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.000f);
	colors[ImGuiCol_DockingPreview] = ImVec4(1.000f, 0.391f, 0.000f, 0.91f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.160f, 0.160f, 0.160f, 1.000f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.469f, 0.469f, 0.469f, 1.000f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.586f, 0.586f, 0.586f, 1.000f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(1.000f, 1.000f, 1.000f, 0.156f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0, 0, 0.000f, 1.000f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.586f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.586f);
	*/

	style->ChildRounding = 4.0f;
	style->FrameBorderSize = 1.0f;
	style->FrameRounding = 2.0f;
	style->GrabMinSize = 7.0f;
	style->PopupRounding = 2.0f;
	style->ScrollbarRounding = 12.0f;
	style->ScrollbarSize = 13.0f;
	style->TabBorderSize = 0.0f;
	style->TabRounding = 10.0f;
	style->WindowRounding = 0.0f;

	style->FrameBorderSize = 0;
}

static bool is_scene_hovered = false;

void override_key_callback(GLFWwindow* window_ptr, int key, int scancode, int action, int mods) {
	ImGui_ImplGlfw_KeyCallback(window_ptr, key, scancode, action, mods);
	Window* window = Window::from(window_ptr);
	
	window->on_key.broadcast({ key, scancode, action, mods });
}

void override_mouse_button_callback(GLFWwindow* window_ptr, int button, int action, int mods) {
	ImGui_ImplGlfw_MouseButtonCallback(window_ptr, button, action, mods);
	Window* window = Window::from(window_ptr);

	window->on_mouse_button.broadcast({ button, action, mods });
}

void Editor::select(int id) {
	this->selected_id = id;
	if (id != -1) {
		this->selected.broadcast(id);
	}
}

wchar_t* to_wide_char(const char* orig) {
	unsigned int newsize = strlen(orig) + 1;
	wchar_t * wcstring = new wchar_t[newsize];
	size_t convertedChars;
	mbstowcs_s(&convertedChars, wcstring, newsize, orig, _TRUNCATE);
	return wcstring;
}

/*
void hotreload(Editor& editor) {
	for (int i = 0; i < editor.num_game_systems; i++) {
		editor.world.systems.pop();
	}

	FreeLibrary(editor.dll_ref);

	register_components_and_systems_t funcs[2];
	editor.dll_ref = load_register_components_and_systems(editor.game_code.c_str(), funcs);

	int num = editor.world.systems.length;
	funcs[1](editor.world);
	editor.num_game_systems = editor.world.systems.length - num;
}
*/

#include "physics/physics.h"

World& get_World(Editor& editor) {
	return editor.world;
}

//string_buffer save_component_to(ComponentStore* store) {
//	return string_buffer("data/") + store->get_component_type()->name + ".ne";
//}

const char* scene_save_path = "data/world_save_file.ne";

void recurisively_register_id(Lister& lister, EntityNode& node) {
	lister.by_id[node.id] = &node;

	for (EntityNode& child : node.children) {
		child.parent = node.id;
		recurisively_register_id(lister, child);
	}
}

bool load_world(Editor& editor, DeserializerBuffer& buffer, const char** err) {
	World& world = get_World(editor);
	ComponentLifetimeFunc* funcs = world.component_lifetime_funcs;

	world.clear();

	read_uint_from_buffer(buffer, world.free_ids.length);
	read_n_from_buffer(buffer, world.free_ids.data, world.free_ids.length * sizeof(ID));

	uint num_archetypes;
	read_uint_from_buffer(buffer, num_archetypes);

	for (uint i = 0; i < num_archetypes; i++) {
		Archetype arch;
		ArchetypeStore store;

		read_u64_from_buffer(buffer, arch);
		read_ArchetypeStore_from_buffer(buffer, store);

		uint entities = store.entity_count_last_block;

		BlockHeader** next_block_chain = &store.blocks;

		printf("Loading archetype %i\n", arch);

		for (uint i = 0; i < store.block_count; i++) {
			BlockHeader* block_header = world.alloc_block();
			*next_block_chain = block_header;

			u8* data = (u8*)(block_header + 1);

			for (uint component_id = 0; component_id < MAX_COMPONENTS; component_id++) {
				if (has_component(arch, component_id)) {
					u8* base_component = data + store.offsets[component_id];
					uint size = world.component_size[component_id];

					if (auto constructor = funcs[component_id].constructor) constructor(base_component, entities); 

					if (auto deserialize_non_trivial = funcs[component_id].deserialize) deserialize_non_trivial(buffer, base_component, entities);
					else read_n_from_buffer(buffer, base_component, world.component_size[component_id] * entities);

					for (uint i = 0; i < entities; i++) {
						ID id = ((Entity*)(data + sizeof(Entity) * i))->id;
						world.id_to_ptr[component_id][id] = base_component + size * i;
					}
				}
			}

			for (uint i = 0; i < entities; i++) {
				ID id = ((Entity*)(data + sizeof(Entity) * i))->id;
				world.id_to_arch[id] = arch;
			}

			next_block_chain = &block_header->next;
			entities = store.max_per_block;
		}

		world.arches.set(arch, store);
	}

	return true;
}

bool load_scene_hierarchy(Lister& lister, DeserializerBuffer& buffer, const char** err) {
	read_EntityNode_from_buffer(buffer, lister.root_node);

	memset(lister.by_id, 0, sizeof(lister.by_id));
	recurisively_register_id(lister, lister.root_node);

	return true;
}

bool load_scene_partition(Renderer& renderer, ScenePartition& partition, DeserializerBuffer& buffer, const char** err) {
	read_n_from_buffer(buffer, &partition, sizeof(ScenePartition));

	//todo this also has to save mesh buckets
	//and generate the various pipelines

	return true;
}

bool load_picking_scene_partition(PickingScenePartition& partition, DeserializerBuffer& buffer, const char** err) {
	read_n_from_buffer(buffer, &partition, sizeof(PickingScenePartition));
	return true;
}

void default_scene(Editor& editor);

bool load_scene(Editor& editor, const char** err) {
	World& world = get_World(editor);
	Renderer& renderer = editor.renderer;

	string_buffer contents;
	if (!io_readf(scene_save_path, &contents)) {
		*err = "Could not read world save path"; 
		return false;
	}

	DeserializerBuffer buffer = {};
	buffer.length = contents.length;
	buffer.data = contents.data;

	if (!load_world(editor, buffer, err)) return false;
	if (!load_scene_hierarchy(editor.lister, buffer, err)) return false;
	if (!load_asset_info(editor.asset_tab.preview_resources, editor.asset_info, buffer, err)) return false;
	//if (!load_scene_partition(editor.renderer.scene_partition, buffer, err)) return false;
	if (!load_picking_scene_partition(editor.picking.partition, buffer, err)) return false;

	auto [_,terrain] = *world.first<Terrain>();

	//todo remove adhoc initialization
	default_terrain(terrain); //generate terrain from height points
	update_terrain_material(renderer.terrain_render_resources, terrain);
	regenerate_terrain(world, renderer.terrain_render_resources, {EDITOR_ONLY});
	
	update_acceleration_structure(renderer.scene_partition, renderer.mesh_buckets, world);


	//submit_framegraph();
}

void on_load(Editor& editor) {
	const char* err = "";
	if (load_scene(editor, &err)) printf("Sucesfully loaded scene!");
	else {
		fprintf(stderr, err);
		default_scene(editor);
	}
}

//todo move into ecs
bool save_world(Editor& editor, SerializerBuffer& buffer, const char** err) {
	World& world = get_World(editor);
	ComponentLifetimeFunc* funcs = world.component_lifetime_funcs;

	write_uint_to_buffer(buffer, world.free_ids.length);
	write_n_to_buffer(buffer, world.free_ids.data, world.free_ids.length * sizeof(ID));

	uint num_archetypes = 0;
	for (int i = 0; i < ARCHETYPE_HASH; i++) {
		if (world.arches.keys.is_full(i)) num_archetypes++;
	}

	write_uint_to_buffer(buffer, num_archetypes);

	//SAVE ECS
	for (int i = 0; i < ARCHETYPE_HASH; i++) {
		if (!world.arches.keys.is_full(i)) continue;

		Archetype arch = world.arches.keys.keys[i];
		ArchetypeStore& store = world.arches.values[i];

		printf("Saving archetype %i\n", arch);

		write_u64_to_buffer(buffer, arch);
		write_ArchetypeStore_to_buffer(buffer, store);

		uint entities = store.entity_count_last_block;
		BlockHeader* block_header = store.blocks;
		
		while (block_header) {
			u8* data = (u8*)(block_header + 1);

			for (uint i = 0; i < MAX_COMPONENTS; i++) {
				if (arch & 1ul << i) {
					u8* base_component = data + store.offsets[i];

					if (auto serialize_non_trivial = funcs[i].serialize) serialize_non_trivial(buffer, base_component, entities);
					else write_n_to_buffer(buffer, base_component, world.component_size[i] * entities);
				}
			}

			block_header = block_header->next;
			entities = store.max_per_block;
		}
	}

	return true;
}

bool save_scene_hierarchy(Lister& lister, SerializerBuffer& buffer, const char** err) {
	write_EntityNode_to_buffer(buffer, lister.root_node);
	return true;
}

bool save_scene_paritition(ScenePartition& partition, SerializerBuffer& buffer, const char** err) {
	write_n_to_buffer(buffer, &partition, sizeof(ScenePartition));
	return true;
}

bool save_picking_scene_partition(PickingScenePartition& partition, SerializerBuffer& buffer, const char** err) {
	write_n_to_buffer(buffer, &partition, sizeof(PickingScenePartition));
	return true;
}

bool save_scene(Editor& editor, const char** err) {
	SerializerBuffer buffer = {};
	buffer.capacity = mb(10);
	buffer.data = TEMPORARY_ARRAY(char, buffer.capacity);

	if (!save_world(editor, buffer, err)) return false;
	if (!save_scene_hierarchy(editor.lister, buffer, err)) return false;
	if (!save_asset_info(editor.asset_tab.preview_resources, editor.asset_info, buffer, err)) return false;
	//if (!save_scene_paritition(editor.renderer.scene_partition, buffer, err)) return false;
	if (!save_picking_scene_partition(editor.picking.partition, buffer, err)) return false;

	if (!io_writef(scene_save_path, { buffer.data, buffer.index })) {
		*err = "Could not write world to save file!";
		return false;
	}

	return true;
}

void on_save(Editor& editor) {
	const char* err = "";
	if (!save_scene(editor, &err)) fprintf(stderr, err);
	else printf("Saved scene");
}

void register_callbacks(Editor& editor, Modules& engine) {
	editor.selected.listeners.clear();
	
	Window& window = *engine.window;
	World& world = *engine.world;


	register_on_inspect_callbacks();

	//editor.asset_tab.register_callbacks(window, editor);

	editor.selected.listen([&engine, &world](ID id) {
		auto rb = world.m_by_id<RigidBody>(id);
		if (rb) {
			rb->bt_rigid_body = NULL; //todo fix leak
		}
	});

	engine.window->override_key_callback(override_key_callback);
	engine.window->override_char_callback(ImGui_ImplGlfw_CharCallback);
	engine.window->override_mouse_button_callback(override_mouse_button_callback);
}

Editor::Editor(Modules& modules, const char* game_code) : 
	renderer(*modules.renderer),
	world(*modules.world),
	window(*modules.window),
	input(*modules.input),
	time(*modules.time),
	game(modules, game_code),
	asset_tab(renderer, asset_info, window),
	copy_of_world(*PERMANENT_ALLOC(World, WORLD_SIZE)),
	actions{world}
{
	//world.add(new DebugShaderReloadSystem());
	//world.add(new TerrainSystem(world, this));
	//world.add(new PickingSystem(*this));

	FramebufferDesc settings{ window.width, window.height };
	add_color_attachment(settings, &scene_view);

	//make_Framebuffer(SceneView, settings);

	icons = {
		{ "play",   load_Texture("editor/play_button3.png") },
		{ "folder", load_Texture("editor/folder_icon.png") },
		{ "shader", load_Texture("editor/shader-works-icon.png") }
	};

	//MainPass* main_pass = renderer.main_pass;
	//main_pass->post_process.append(&picking_pass);



	modules.input->capture_mouse(false);

	init_actions(actions);

	editor_viewport = {};
	editor_viewport.input = Input();
	editor_viewport.contents = renderer.scene_map;
	editor_viewport.type = EDITOR_VIEWPORT_TYPE_SCENE;
	editor_viewport.name = "Scene";

	Dependency dependencies[2] = {
		{ FRAGMENT_STAGE, RenderPass::Scene },
		{ FRAGMENT_STAGE, PreviewPass },
	};

	build_framegraph(renderer, { dependencies, 2});

	init_imgui();
	register_callbacks(*this, modules);
	editor_viewport.input.init(window);

	game.init();

	ImGui_ImplVulkan_CreateDeviceObjects();

	make_special_gizmo_resources(gizmo_resources);

	Profiler::begin_frame();
	on_load(*this);
	Profiler::end_frame();
}

void Editor::init_imgui() {
	// Setup ImGui binding
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigDockingWithShift = false;

	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

	ImFontConfig icons_config; icons_config.MergeMode = false; icons_config.PixelSnapH = true; icons_config.FontDataOwnedByAtlas = true; icons_config.FontDataSize = 32.0f;

	auto font_path = tasset_path("fonts/segoeui.ttf");
	ImFont* font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 32.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());

	io.FontAllowUserScaling = true;

	asset_tab.explorer.filename_font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 26.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());
	asset_tab.explorer.default_font = font;

	for (int i = 0; i < 10; i++) {
		ImFontConfig im_font_config; im_font_config.OversampleH = 5; im_font_config.OversampleV = 5;

		shader_editor.font[i] = io.Fonts->AddFontFromFileTTF(font_path.c_str(), (i + 1) * 10.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());
	}

	set_darcula_theme();

	ImGui_ImplGlfw_InitForOpenGL(window.window_ptr, false);
	ImGui_ImplVulkan_Init();

}

texture_handle Editor::get_icon(string_view name) {
	for (auto& icon : icons) {
		if (icon.name == name) {
			return icon.texture_id;
		}
	}
	
	throw "Could not find icon";
}

void ImGui::InputText(const char* str, sstring& buffer) {
	char buf[50];
	std::memcpy(buf, buffer.data, buffer.length());
	buf[buffer.length()] = '\0';
	ImGui::InputText(str, buf, 50);
	buffer = buf;
}

void ImGui::InputText(const char* str, string_buffer& buffer) {
	char buf[50];
	std::memcpy(buf, buffer.data, buffer.length + 1);
	ImGui::InputText(str, buf, 50);
	buffer = buf;
}

void default_scene(Editor& editor) {
	World& world = get_World(editor);

	editor.asset_info.default_material = make_new_material(editor.asset_tab, editor);

	/*
	{
		auto [e, trans, model_renderer, materials] = world.make<Transform, ModelRenderer, Materials>();
		trans.position.z = -4;
		trans.position.y = 1;
		trans.position.x = -2;
		trans.rotation = glm::normalize(glm::angleAxis(glm::radians(90.0f), glm::vec3(-1, 0, 0)));
		//trans->position = glm::vec3(0, -10, 0);

		editor.select(e.id);

		model_renderer.model_id = import_model(editor.asset_tab, "HOVERTANK.fbx");
		materials.materials.append(editor.asset_info.default_material);

		register_entity(editor.lister, "Plane", e.id);
	}*/


	//Ground Plane
	/*{
		auto[e, trans, mr, materials] = world.make<Transform, ModelRenderer, Materials>();

		trans.position = glm::vec3(-5, 0, 0);
		trans.scale = glm::vec3(5);
		mr.model_id = load_Model("subdivided_plane8.fbx");
		materials.materials.append(editor.asset_tab.default_material);

		register_entity(editor.lister, "Ground", e.id);
	}*/ 

	{
		auto[e, trans, terrain] = world.make<Transform, Terrain>();

		default_terrain(terrain);
		update_terrain_material(editor.renderer.terrain_render_resources, terrain);
		clear_terrain(editor.renderer.terrain_render_resources);

		register_entity(editor.lister, "Terrain", e.id);
	}

	//Light
	{
		auto[e, trans, dir_light] = world.make<Transform, DirLight>();
		register_entity(editor.lister, "Sun Light", e.id);
	}

	{
		auto[e, trans, flyover, camera] = world.make<Transform, Flyover, Camera>(EDITOR_ONLY);
		register_entity(editor.lister, "Camera", e.id);
	}

	/*
	{
		auto id = world.make_ID();
		auto e = world.make<Entity>(id);
		auto trans = world.make<Transform>(id);
		auto sky = world.make<Skybox>(id);
		auto entity_editor = world.make<EntityEditor>(id);
		entity_editor->name = "Skybox";

		ID id = editor.asset_tab.assets.make_ID();
		MaterialAsset* asset = editor.asset_tab.assets.make<MaterialAsset>(id);
		asset->name = "Skybox Material";
		asset->handle = world.by_id<Materials>(world.id_of(skybox))->materials[0];

		register_new_material(world, asset_tab, *this, render_ctx, id);
	}

	{

	}

	{
		ID id = editor.asset_tab.assets.make_ID();
		MaterialAsset* asset = editor.asset_tab.assets.make<MaterialAsset>(id);
		asset->name = "Skybox Material";
		asset->handle = world.by_id<Materials>(world.id_of(skybox))->materials[0];

		register_new_material(world, asset_tab, *this, render_ctx, id);
	}*/
	
}
void render_view(Editor& editor, World& world, RenderPass& ctx) {
	if (ctx.type == RenderPass::Scene && editor.selected_id != -1) {
		ID selected = editor.selected_id;
		render_object_selected_outline(editor.outline_selected, editor.world, selected, ctx);
	}
	//editor.picking_pass.render(world, ctx);
	//render_settings.render_features.append(new DebugShaderReloadSystem());
	//render_settings.render_features.append(new PickingSystem(world));

	//world.add(new Store<EntityEditor>(100));
	//world.add(new DebugShaderReloadSystem());
	//world.add(new TerrainSystem(world, this));
	//world.add(new PickingSystem(*this));
}

void render_overlay(Editor& editor, RenderPass& ctx) {
	editor.picking.visualize(editor.editor_viewport.viewport, editor.editor_viewport.input.mouse_position, ctx);
}

void render_Editor(Editor& editor, RenderPass& ctx, RenderPass& scene);

void render_frame(Editor& editor, World& world) {
	Renderer& renderer = editor.renderer;
	FrameData frame_data;

	Viewport viewport = editor.editor_viewport.viewport; 
	EntityQuery mask = editor.playing_game ? EntityQuery().with_none(EDITOR_ONLY) : EntityQuery();

	GizmoRenderData gizmo_render_data = {};

	{
		update_camera_matrices(world, editor.playing_game ? mask : EntityQuery{EDITOR_ONLY}, viewport);
		extract_render_data(renderer, viewport, frame_data, world, mask);
		extract_render_data_special_gizmo(gizmo_render_data, world, mask);
	}

	//SYNC WITH FRAMES IN FLIGHT
	GPUSubmission gpu_submission = build_command_buffers(renderer, frame_data);
	
	//TEMPORARY SOLUTION TO TERRAIN INITIALIZATION PROBLEM
	{
		static uint receive_terrain = 0;
		uint frame_index = get_frame_index();
		if (receive_terrain == 0 && frame_index == 0) {
			receive_terrain = 1;
		}
		else if (receive_terrain == 1 && frame_index == 0) {
			auto[_, terrain] = *world.first<Terrain>();
			receive_generated_heightmap(renderer.terrain_render_resources, terrain);
			regenerate_terrain(world, renderer.terrain_render_resources, EntityQuery{ EDITOR_ONLY }); //this should not be necessary 
			receive_terrain = 2;
		}
	}

	RenderPass& scene_pass = gpu_submission.render_passes[RenderPass::Scene];

	if (!editor.playing_game) {
		//material_handle mat_handle = editor.gizmo_resources.camera_material; //FRANKLY THIS SHOULD NOT BE NECESSARY
		//bind_descriptor(scene_pass.cmd_buffer, 0, editor.renderer.scene_pass_descriptor[get_frame_index()]);

		//bind_pipeline(scene_pass.cmd_buffer, query_pipeline(mat_handle, scene_pass.cmd_buffer.render_pass, 1.0));
		//bind_descriptor(scene_pass.cmd_buffer, 1, editor.renderer.lighting_system.pbr_descriptor);

		render_special_gizmos(editor.gizmo_resources, gizmo_render_data, scene_pass);
		render_overlay(editor, scene_pass);
	}

	render_skybox(frame_data.skybox_data, scene_pass);
	render_Editor(editor, gpu_submission.render_passes[RenderPass::Screen], scene_pass);


	submit_frame(renderer, gpu_submission);

	/*
	if (editor.game_fullscreen && editor.playing_game) {
		main_pass.output.bind();
		main_pass.output.clear_color(glm::vec4(0, 0, 0, 1));
		main_pass.output.clear_depth(glm::vec4(0, 0, 0, 1));

		main_pass.render(world, ctx);
		return;
	}

	main_pass.render_to_buffer(world, ctx, [this]() {
		editor.scene_view_fbo.bind();
		editor.scene_view_fbo.clear_color(glm::vec4(0, 0, 0, 1));
		editor.scene_view_fbo.clear_depth(glm::vec4(0, 0, 0, 1));
	});

	main_pass.output.bind();
	main_pass.output.clear_color(glm::vec4(0, 0, 0, 1));
	main_pass.output.clear_depth(glm::vec4(0, 0, 0, 1));
	*/
}

void on_set_play_mode(Editor& editor, bool playing) {
	editor.playing_game = playing;

	if (playing) { //this will not fully serialize this
		editor.copy_of_world = editor.world;
	}
	else {
		editor.world = editor.copy_of_world;
		editor.editor_viewport.input.capture_mouse(false);
	}
}

glm::vec3 Editor::place_at_cursor() {
	RayHit hit;
	hit.position = glm::vec3();
	picking.ray_cast(editor_viewport.viewport, editor_viewport.input.mouse_position, hit);

	return hit.position;
}

void spawn_Model(World& world, Editor& editor, model_handle model_handle) { //todo maybe move to assetTab
	AssetNode* node = editor.asset_info.asset_type_handle_to_node[AssetNode::Model][model_handle.id];
	ModelAsset* model_asset = node ? &node->model : nullptr;
		
	if (model_asset) {
		auto[e, trans, model_renderer, materials] = world.make<Transform, ModelRenderer, Materials>();
		materials.materials = slice<material_handle>(model_asset->materials);
		model_renderer.model_id = model_asset->handle;

		trans.position = editor.place_at_cursor();
		editor.create_new_object(model_asset->name, e.id);
	}
}

void Editor::begin_imgui(Input& input) {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (input.mouse_captured) {
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
	}
	else {
		ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	}

	unsigned char* pixels;
	int width, height;
	ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	ImFontAtlas* atlas = ImGui::GetIO().Fonts;

	ImGui::PushFont(atlas->Fonts[0]);
}

void Editor::end_imgui(CommandBuffer& cmd_buffer) {
	Profile render_imgui("Render imgui");

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(cmd_buffer, ImGui::GetDrawData());

	render_imgui.end();
}



void render_play(Editor& editor) {
	if (ImGui::ImageButton(editor.get_icon("play"), ImVec2(30, 30))) {
		on_set_play_mode(editor, true);
	}

	ImGui::SameLine();
	
	ImGui::SetNextItemWidth(10.0);
	ImGui::Checkbox("Game Fullscreen", &editor.game_fullscreen);
	//ImGui::PopStyleVar();
}

//todo remove editor ptr!
void render_Viewport(Editor& editor, EditorViewport& editor_viewport) {
	Input& input = editor_viewport.input;
	World& world = get_World(editor);

	//ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_NoTabBar;

	//ImGui::DockSpace(ImGui::GetID("controls"), ImVec2(0,0), ImGuiDockNodeFlags_NoTabBar);

	//ImGui::End();

	//ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
	if (ImGui::Begin(editor_viewport.name.data, NULL, ImGuiWindowFlags_NoScrollbar)) {
		float width = ImGui::GetContentRegionMax().x;
		float height = ImGui::GetContentRegionMax().y - ImGui::GetCursorPos().y;

		input.region_min.x = ImGui::GetWindowPos().x + ImGui::GetCursorPos().x;
		input.region_min.y = ImGui::GetWindowPos().y + ImGui::GetCursorPos().y;
		input.region_max = input.region_min + glm::vec2(width, height);

		editor_viewport.viewport.x = input.region_min.x;
		editor_viewport.viewport.y = input.region_min.y;
		editor_viewport.viewport.width = width;
		editor_viewport.viewport.height = height;

		update_camera_matrices(world, EntityQuery{ EDITOR_ONLY }, editor_viewport.viewport);


		ImGui::Image(editor_viewport.contents, ImVec2(width, height));

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_AND_DROP_MODEL")) {
				spawn_Model(world, editor, *(model_handle*)payload->Data);
			}
			ImGui::EndDragDropTarget();
		}


		bool is_scene_hovered = ImGui::IsItemHovered();

		if (!editor.playing_game) {
			editor.gizmo.render(world, editor, editor.editor_viewport.viewport, input);
		}
	}
	

	ImGui::End();
}

void render_Editor(Editor& editor, RenderPass& ctx, RenderPass& scene) {
	Input& input = editor.input;
	
	editor.begin_imgui(input);

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::SetNextWindowBgAlpha(0.0f);

	//ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize | 
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	bool p_open;
	
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace Demo", &p_open, window_flags);
	ImGui::PopStyleVar(3);

	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("File")) {
		ImGui::MenuItem("New Scene", "CTRL+N");
		ImGui::MenuItem("Open Scene");
		ImGui::MenuItem("Recents");
		if (ImGui::MenuItem("Save", "CTRL+S")) {
			on_save(editor);
		}
		if (ImGui::MenuItem("Exit", "ALT+F4")) {
			log("Exiting");
			editor.exit = true;
		}
		ImGui::EndMenu();
	}
	
	if (ImGui::BeginMenu("Edit")) {
		if (ImGui::MenuItem("Undo", "CTRL+Z")) undo_action(editor.actions);
		if (ImGui::MenuItem("Redo", "CTRL+R")) redo_action(editor.actions);

		ImGui::MenuItem("Copy", "CTRL+C");
		ImGui::MenuItem("Paste", "CTRL+V");
		ImGui::MenuItem("Duplicate", "CTRL+D");
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Settings")) {
		ImGui::EndMenu();
	}

	render_play(editor);

	ImGui::EndMainMenuBar();
	ImGui::PopStyleVar();

	ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
	ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	
	ImGui::End();	

	render_Viewport(editor, editor.editor_viewport);

	//==========================

	Profile render_editor("Render Editor");

	World& world = get_World(editor);

	editor.display_components.render(world, ctx, editor);
	editor.lister.render(world, editor, ctx);

	editor.asset_tab.render(world, editor, ctx);
	editor.shader_editor.render(world, editor, ctx, editor.input);
	editor.profiler.render(world, editor, ctx);

	ImGui::PopFont();
	ImGui::EndFrame();

	render_editor.end();

	editor.end_imgui(ctx.cmd_buffer);
}

void Editor::create_new_object(string_view name, ID id) {
	entity_create_action(actions, id);
	register_entity(lister, name, id);
	select(id);
}

Editor::~Editor() {
	log("destructor");
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void delete_object(Editor& editor) {
	World& world = get_World(editor);

	if (editor.selected_id >= 0) {
		entity_destroy_action(editor.actions, editor.selected_id);
		editor.selected_id = -1;
	}
}

void mouse_click_select(Editor& editor) {
	World& world = get_World(editor);
	EditorViewport& viewport = editor.editor_viewport;
	
	int selected = editor.picking.pick(viewport.viewport, viewport.input.mouse_position);
	editor.select(selected);
}

void respond_to_shortcut(Editor& editor) {
	Input& input = editor.editor_viewport.input;
	World& world = get_World(editor);

	if (ImGui::GetIO().WantCaptureKeyboard) {
		input.active = false;
		input.clear();
	}

	if (input.key_pressed(Key::P)) {
		on_set_play_mode(editor, !editor.playing_game);
	}

	if (editor.game_fullscreen && editor.playing_game) return;

	if (input.key_pressed(Key::X)) delete_object(editor);
	if (input.mouse_button_pressed(MouseButton::Left) && !ImGuizmo::IsOver()) mouse_click_select(editor);
	if (input.key_mod_pressed(Key::Y)) undo_action(editor.actions);
	if (input.key_mod_pressed(Key::R)) redo_action(editor.actions);
	if (input.key_mod_pressed(Key::S)) on_save(editor);

	UpdateCtx ctx(editor.time, input);
	editor.gizmo.update(world, editor, ctx);
}

void respond_to_framediffs(Editor& editor) {
	World& world = editor.world;
	ActionStack& stack = editor.actions.frame_diffs;

	Archetype terrain_control_point = (1ull << type_id<TerrainControlPoint>()) | (1ull << type_id<TerrainSplat>());
	bool rebuild_acceleration = false;
	bool update_terrain = false;

	for (EditorActionHeader header : stack.stack) {
		switch (header.type) {
		case EditorActionHeader::Diff: {
			Diff* diff = (Diff*)header.ptr;

			if (diff->type->name == "Transform") {
				rebuild_acceleration = true;
			}

			if (diff->type->name == "Materials") {
				rebuild_acceleration = true; //todo requires changing mesh bucket of element
			}

			if (diff->type->name == "MaterialDesc") {
				assert(diff->id != INVALID_HANDLE);
				editor.renderer.update_materials.append({ diff->id, *(MaterialDesc*)diff->copy_ptr.data(), *(MaterialDesc*)diff->real_ptr });
			}

			Archetype arch = world.arch_of_id(diff->id);

			if (arch & terrain_control_point) update_terrain = true;

			break;
		}

		case EditorActionHeader::Create_Entity: {
			EntityCopy* copy = (EntityCopy*)header.ptr;
			rebuild_acceleration = true;

			//it shouldn't be necessary to check both from and to, since when creating a framediff the header could be set accordingly
			if (copy->from & terrain_control_point || copy->to & terrain_control_point) update_terrain = true;

			break;
		}

		case EditorActionHeader::Destroy_Entity: {
			EntityCopy* copy = (EntityCopy*)header.ptr;
			rebuild_acceleration = true;

			//it shouldn't be necessary to check both from and to, since when creating a framediff the header could be set accordingly
			if (copy->from & terrain_control_point || copy->to & terrain_control_point) update_terrain = true;

			break;
		}

		case EditorActionHeader::Create_Component:
			//todo handle updating terrain
			break;

		case EditorActionHeader::Destroy_Component:
			break;
		}
	}

	if (rebuild_acceleration) {
		Renderer& renderer = editor.renderer;
		bool is_static = true;

		renderer.scene_partition.node_count = 0;
		editor.picking.partition.node_count = 0;
		renderer.scene_partition.count = 0;
		editor.picking.partition.count = 0;

		editor.picking.rebuild_acceleration_structure(editor.world);
		build_acceleration_structure(renderer.scene_partition, renderer.mesh_buckets, editor.world);
	}

	if (update_terrain) {
		regenerate_terrain(editor.world, editor.renderer.terrain_render_resources, EntityQuery{EDITOR_ONLY});
	}

	clear_stack(editor.actions.frame_diffs);
}

#include "graphics/rhi/rhi.h"

//Application
APPLICATION_API Editor* init(const char* args, Modules& modules) {
	Editor* editor = new Editor(modules, args);

	editor->editor_viewport.viewport.width = modules.window->width;
	editor->editor_viewport.viewport.height = modules.window->height;

	Renderer& renderer = editor->renderer;
	end_gpu_upload();

	return editor;
}

APPLICATION_API bool is_running(Editor* editor, Modules& engine) {
	return !engine.window->should_close() && !editor->exit;
}

APPLICATION_API void update(Editor& editor, Modules& modules) {
	respond_to_shortcut(editor);

	UpdateCtx update_ctx(editor.time, editor.editor_viewport.input);
	UpdateCtx update_ctx_editor_only = update_ctx;
	update_ctx_editor_only.layermask = EntityQuery{ EDITOR_ONLY };

	if (editor.playing_game) {
		if (update_ctx.input.key_down(Key::R)) editor.game.reload();
		
		editor.game.engine.input = &update_ctx.input;
		editor.game.update();
	}
	else {
		update_local_transforms(editor.world, update_ctx);
		update_flyover(editor.world, update_ctx_editor_only);
		edit_Terrain(editor, editor.world, update_ctx);
		//FlyOverSystem::update(engine.world, update_ctx);
	}

	respond_to_framediffs(editor);
}

#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/transforms.h"

APPLICATION_API void reload(Editor& editor, Modules& modules) {
	register_callbacks(editor, modules);
}

APPLICATION_API void render(Editor& editor, Modules& engine) {
	World& world = get_World(editor);

	render_frame(editor, world);
	editor.editor_viewport.input.clear();
}

APPLICATION_API void deinit(Editor* editor) {
	delete editor;
}