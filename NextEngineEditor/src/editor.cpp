﻿#include "editor.h"
#include "graphics/rhi/window.h"
#include "core/io/input.h"
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
#include "core/io/vfs.h"
#include "graphics/assets/material.h"
#include "core/serializer.h"
#include "terrain.h"
#include <ImGuizmo/ImGuizmo.h>
#include "core/profiler.h"
#include "graphics/renderer/renderer.h"
#include "engine/engine.h"
#include "graphics/assets/assets.h"
#include <imgui/imgui_internal.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


//theme by Derydoca 
void set_darcula_theme() {
	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;

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

/*
void on_load_world(Editor& editor) {
	World& world = get_World(editor);

	unsigned int save_files_available = 0;

	for (int i = 0; i < world.components_hash_size; i++) {
		auto store = world.components[i].get();
		if (store != NULL) {
			string_buffer component_path = save_component_to(store);
			
			string_buffer data_buffer;
			if (!io_readfb(component_path, &data_buffer)) continue;

			DeserializerBuffer buffer{ data_buffer.data, data_buffer.length };
			refl::Type* component_type = store->get_component_type();

			uint num_components;
			read_from_buffer(buffer, &num_components); //.read_int();
			
			for (unsigned int i = 0; i < num_components; i++) {
				unsigned int id = buffer.read_int();

				if (id == 0 && false) {
					id = world.make_ID();
					void* ptr = store->make_by_id(id);



					buffer.read(component_type, ptr);

					store->free_by_id(id);
				}
				else {
					void* ptr = store->make_by_id(id);

					buffer.read(component_type, ptr);
					world.skipped_ids.append(id);
				}
			}

			save_files_available++;
		}
	}

	//if (save_files_available == 0) default_scene(editor);
}*/

void default_scene(Editor& editor);

void on_load(Editor& editor) {
	World& world = get_World(editor);

	default_scene(editor);
	editor.picking.rebuild_acceleration_structure(world);
	return;

	//on_load_world(editor);

	//todo call callbacks
	//world.get<Skybox>()->fire_callbacks();

	editor.asset_tab.on_load(world);

	editor.picking.rebuild_acceleration_structure(world);
}

void on_save_world(Editor& editor) {
	World& world = get_World(editor);

	/*
	for (int i = 0; i < world.components_hash_size; i++) {
		auto store = world.components[i].get();
		if (store != NULL) {
			SerializerBuffer buffer;

			vector<Component> filtered;
			for (Component& comp : store->filter_untyped()) {
				if (world.by_id<EntityEditor>(comp.id))
					filtered.append(comp);
			}

			write_to_buffer(buffer, filtered.length);

			log("Serialized ", filtered.length, " ", store->get_component_type()->name);

			for (Component& comp : filtered) {
				write_to_buffer(buffer, comp.id);
				//buffer.write_int(comp.id);
				//buffer.write(comp.type, comp.data);
			}

			string_buffer component_save_path = save_component_to(store);
			if (!io_writef(component_save_path, { buffer.data, buffer.index })) {
				throw "Could not save data";
			}
		}
	}*/
}

void on_save(Editor& editor) {
	on_save_world(editor);
	editor.asset_tab.on_save();
}

void register_callbacks(Editor& editor, Modules& engine) {
	editor.selected.listeners.clear();
	
	Window& window = *engine.window;
	World& world = *engine.world;


	register_on_inspect_callbacks();

	//editor.asset_tab.register_callbacks(window, editor);

	editor.selected.listen([&engine, &world](ID id) {
		auto rb = world.by_id<RigidBody>(id);
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

	on_load(*this);

	modules.input->capture_mouse(false);

	init_actions(actions);

	//engine.asset_manager.shaders.load("shaders/pbr.vert", "shaders/paralax_pbr.frag");

	//render_ctx.dir_light = get_dir_light(world, render_ctx.layermask);

	/*
	*/

	//PreRenderParams pre_render_ctx(GAME_LAYER);
	//render_ctx.dir_light = get_dir_light(world, render_ctx.layermask);

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

	editor.asset_tab.default_material = create_new_material(editor.asset_tab, editor);

	{
		auto [e, trans, model_renderer, materials] = world.make<Transform, ModelRenderer, Materials>();
		trans.position.z = -4;
		trans.position.y = 1;
		trans.position.x = -2;
		trans.rotation = glm::normalize(glm::angleAxis(glm::radians(90.0f), glm::vec3(-1, 0, 0)));
		//trans->position = glm::vec3(0, -10, 0);

		editor.select(e.id);

		model_renderer.model_id = import_model(editor.asset_tab, "HOVERTANK.fbx");
		materials.materials.append(editor.asset_tab.default_material);

		register_entity(editor.lister, "Plane", e.id);
	}


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

		gen_terrain(terrain);
		update_terrain_material(editor.renderer.terrain_render_resources, terrain);

		register_entity(editor.lister, "Terrain", e.id);
	}

	//Light
	{
		auto[e, trans, dir_light] = world.make<Transform, DirLight>();
		register_entity(editor.lister, "Sun Light", e.id);
	}

	{
		auto[e, trans, flyover, camera] = world.make<Transform, Flyover, Camera>();
		e.layermask |= EDITOR_LAYER;

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
	editor.picking.visualize(editor.world, editor.editor_viewport.input, ctx);
}

void render_Editor(Editor& editor, RenderPass& ctx, RenderPass& scene);

void render_frame(Editor& editor, World& world) {
	Renderer& renderer = editor.renderer;
	FrameData frame_data;

	Viewport viewport = editor.editor_viewport.viewport; 
	Layermask mask = editor.playing_game ? GAME_LAYER : GAME_LAYER | EDITOR_LAYER;

	{
		extract_render_data(renderer, viewport, frame_data, world, mask);
	}

	GPUSubmission gpu_submission = build_command_buffers(renderer, frame_data);

	render_Editor(editor, gpu_submission.render_passes[RenderPass::Screen], gpu_submission.render_passes[RenderPass::Scene]);
	render_overlay(editor, gpu_submission.render_passes[RenderPass::Scene]);

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
	picking.ray_cast(world, editor_viewport.input, hit);

	return hit.position;
}

void spawn_Model(World& world, Editor& editor, model_handle model_handle) { //todo maybe move to assetTab
	AssetNode* node = editor.asset_info.asset_type_handle_to_node[AssetNode::Model][model_handle.id];
	ModelAsset* model_asset = node ? &node->model : nullptr;
		
	if (model_asset) {
		auto[e, trans, model_renderer, materials] = world.make<Transform, ModelRenderer, Materials>();
		materials.materials = model_asset->materials;
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

		ID cam = get_camera(world, EDITOR_LAYER);
		update_camera_matrices(world, cam, editor_viewport.viewport);


		ImGui::Image(editor_viewport.contents, ImVec2(width, height));

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_AND_DROP_MODEL")) {
				spawn_Model(world, editor, *(model_handle*)payload->Data);
			}
			ImGui::EndDragDropTarget();
		}


		bool is_scene_hovered = ImGui::IsItemHovered();

		editor.gizmo.render(world, editor, editor.editor_viewport.viewport, input);
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
	Input& input = editor.editor_viewport.input;
	
	int selected = editor.picking.pick(world, input);
	editor.select(selected);
}

void respond_to_shortcut(Editor& editor) {
	Input& input = editor.editor_viewport.input;
	World& world = get_World(editor);

	if (input.key_pressed(GLFW_KEY_P)) {
		on_set_play_mode(editor, !editor.playing_game);
	}

	if (editor.game_fullscreen && editor.playing_game) return;

	if (input.key_pressed(GLFW_KEY_X)) delete_object(editor);
	if (input.mouse_button_pressed(MouseButton::Left) && !ImGuizmo::IsOver()) mouse_click_select(editor);
	if (input.key_pressed(89, true) && input.key_down(GLFW_KEY_LEFT_CONTROL, true)) undo_action(editor.actions);
	if (input.key_pressed('R', true) && input.key_down(GLFW_KEY_LEFT_CONTROL, true)) redo_action(editor.actions);
	if (input.key_pressed('S', true) && input.key_down(GLFW_KEY_LEFT_CONTROL, true)) on_save(editor);

	UpdateCtx ctx(editor.time, input);
	editor.gizmo.update(world, editor, ctx);
}

void respond_to_framediffs(Editor& editor) {
	World& world = editor.world;
	ActionStack& stack = editor.actions.frame_diffs;

	Archetype terrain_control_point = to_archetype<TerrainControlPoint, TerrainSplat>();


	for (EditorActionHeader header : stack.stack) {
		bool rebuild_acceleration = false;
		bool update_terrain = false;
		
		switch (header.type) {
		case EditorActionHeader::Diff: {
			Diff* diff = (Diff*)header.ptr;

			if (diff->type->name == "Transform") {
				rebuild_acceleration = true;
			}

			if (diff->type->name == "MaterialDesc") {
				MaterialDesc* material_desc = (MaterialDesc*)diff->real_ptr;
				replace_Material({diff->id}, *material_desc);
				// update material
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
			regenerate_terrain(editor.world, editor.renderer.terrain_render_resources, EDITOR_LAYER);
		}
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
	update_ctx.layermask = editor.playing_game ? GAME_LAYER : EDITOR_LAYER;

	if (editor.playing_game) {
		if (update_ctx.input.key_down('R')) editor.game.reload();
		editor.game.update();
	}
	else {
		update_flyover(editor.world, update_ctx);
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

	Layermask layermask = editor.playing_game ? GAME_LAYER : GAME_LAYER | EDITOR_LAYER;

	render_frame(editor, world);
	editor.editor_viewport.input.clear();
}

APPLICATION_API void deinit(Editor* editor) {
	delete editor;
}