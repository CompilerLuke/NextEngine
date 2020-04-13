#include "stdafx.h"
#include "editor.h"
#include "graphics/rhi/window.h"
#include "core/io/input.h"
#include "graphics/renderer/renderer.h"
#include "core/io/logger.h"
#include "core/reflection.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

DEFINE_APP_COMPONENT_ID(EntityEditor, 15)

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

string_buffer save_component_to(ComponentStore* store) {
	return string_buffer("data/") + store->get_component_type()->name + ".ne";
}

void on_load_world(Editor& editor) {
	World& world = get_World(editor);
	Assets& assets = editor.asset_manager;

	unsigned int save_files_available = 0;

	for (int i = 0; i < world.components_hash_size; i++) {
		auto store = world.components[i].get();
		if (store != NULL) {
			string_buffer component_path = save_component_to(store);
			
			string_buffer data_buffer;
			if (!readfb(assets, component_path, &data_buffer)) continue;

			DeserializerBuffer buffer(data_buffer.data, data_buffer.length);
			reflect::TypeDescriptor* component_type = store->get_component_type();

			unsigned num_components = buffer.read_int();
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
}

void default_scene(Editor& editor, RenderCtx& ctx);

void on_load(Editor& editor, RenderCtx& ctx) {
	World& world = get_World(editor);

	default_scene(editor, ctx);
	return;

	on_load_world(editor);

	world.get<Skybox>()->fire_callbacks();
	ctx.skybox = world.filter<Skybox>()[0];

	editor.asset_tab.on_load(world, ctx);

	editor.picking.rebuild_acceleration_structure(world);
}

void on_save_world(Editor& editor) {
	World& world = get_World(editor);
	Assets& assets = editor.asset_manager;

	for (int i = 0; i < world.components_hash_size; i++) {
		auto store = world.components[i].get();
		if (store != NULL) {
			SerializerBuffer buffer;

			vector<Component> filtered;
			for (Component& comp : store->filter_untyped()) {
				if (world.by_id<EntityEditor>(comp.id))
					filtered.append(comp);
			}

			buffer.write_int(filtered.length);

			log("Serialized ", filtered.length, " ", store->get_component_type()->name);

			for (Component& comp : filtered) {
				buffer.write_int(comp.id);
				buffer.write(comp.type, comp.data);
			}

			string_buffer component_save_path = save_component_to(store);
			if (!writef(assets, component_save_path, { buffer.data, buffer.index })) {
				throw "Could not save data";
			}
		}
	}
}

void on_save(Editor& editor) {
	on_save_world(editor);
	editor.asset_tab.on_save();
}

void register_callbacks(Editor& editor, Modules& engine) {
	Window& window = *engine.window;
	World& world = *engine.world;
	
	editor.selected.listeners.clear();

	register_on_inspect_callbacks();

	editor.asset_tab.register_callbacks(window, editor);

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
	asset_manager(*modules.assets),
	renderer(*modules.renderer),
	world(*modules.world),
	window(*modules.window),
	input(*modules.input),
	time(*modules.time),

	picking(asset_manager),
	outline_selected(asset_manager),
	game(modules, game_code),
	asset_tab(renderer, asset_manager, window),
	copy_of_world(*PERMANENT_ALLOC(World))
{
	//world.add(new DebugShaderReloadSystem());
	//world.add(new TerrainSystem(world, this));
	//world.add(new PickingSystem(*this));

	init_imgui();
	register_callbacks(*this, modules);

	AttachmentDesc color_attachment(scene_view);
	color_attachment.min_filter = Filter::Linear;
	color_attachment.mag_filter = Filter::Linear;

	FramebufferDesc settings;
	settings.width = window.width;
	settings.height = window.height;
	settings.color_attachments.append(color_attachment);

	scene_view_fbo = Framebuffer(asset_manager, settings);

	icons = {
		{ "play",   load_Texture(asset_manager, "editor/play_button3.png") },
		{ "folder", load_Texture(asset_manager, "editor/folder_icon.png") },
		{ "shader", load_Texture(asset_manager, "editor/shader-works-icon.png") }
	};

	MainPass* main_pass = renderer.main_pass;
	//main_pass->post_process.append(&picking_pass);

	CommandBuffer cmd_buffer(asset_manager);
	RenderCtx render_ctx(cmd_buffer, main_pass);
	render_ctx.layermask = GAME_LAYER | EDITOR_LAYER;
	render_ctx.width = window.width;
	render_ctx.height = window.height;

	on_load(*this, render_ctx); //todo remove any rendering from load

	modules.input->capture_mouse(false);

	//engine.asset_manager.shaders.load("shaders/pbr.vert", "shaders/paralax_pbr.frag");

	render_ctx.dir_light = get_dir_light(world, render_ctx.layermask);

	/*
	*/

	PreRenderParams pre_render_ctx(GAME_LAYER);

	render_ctx.dir_light = get_dir_light(world, render_ctx.layermask);

	game.init();
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

	auto font_path = tasset_path(asset_manager, "fonts/segoeui.ttf");
	ImFont* font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 32.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());

	io.FontAllowUserScaling = true;

	asset_tab.filename_font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 26.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());
	asset_tab.default_font = font;

	for (int i = 0; i < 10; i++) {
		ImFontConfig im_font_config; im_font_config.OversampleH = 5; im_font_config.OversampleV = 5;

		shader_editor.font[i] = io.Fonts->AddFontFromFileTTF(font_path.c_str(), (i + 1) * 10.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());
	}

	set_darcula_theme();

	ImGui_ImplGlfw_InitForOpenGL(window.window_ptr, false);
	ImGui_ImplOpenGL3_Init();

}

texture_handle Editor::get_icon(string_view name) {
	for (auto& icon : icons) {
		if (icon.name == name) {
			return icon.texture_id;
		}
	}
	
	throw "Could not find icon";
}

void ImGui::InputText(const char* str, string_buffer& buffer) {
	char buf[50];
	std::memcpy(buf, buffer.c_str(), buffer.size() + 1);
	ImGui::InputText(str, buf, 50);
	buffer = string_view(buf);
}

void default_scene(Editor& editor, RenderCtx& ctx) {
	World& world = get_World(editor);
	Assets& assets = editor.asset_manager;

	editor.asset_tab.default_material = create_new_material(world, editor.asset_tab, editor, ctx)->handle;

	auto model_renderer_id = world.make_ID();

	{
		auto id = model_renderer_id;
		auto e = world.make<Entity>(id);
		auto trans = world.make<Transform>(id);
		trans->position.z = -5;

		editor.select(id);

		auto name = world.make<EntityEditor>(id);
		name->name = "Plane";
	}

	auto model_render = world.make<ModelRenderer>(model_renderer_id);
	model_render->model_id = load_Model(assets, "HOVERTANK.fbx");

	auto materials = world.make<Materials>(model_renderer_id);
	materials->materials.append(editor.asset_tab.default_material);

	//Ground Plane
	{
		auto id = world.make_ID();
		auto e = world.make<Entity>(id);
		auto name = world.make<EntityEditor>(id);
		name->name = "Ground";

		auto trans = world.make<Transform>(id);
		trans->position = glm::vec3(-5, 0, 0);
		trans->scale = glm::vec3(5);
		auto mr = world.make<ModelRenderer>(id);
		mr->model_id = load_Model(assets, "subdivided_plane8.fbx");

		auto materials = world.make<Materials>(id);
		materials->materials.append(editor.asset_tab.default_material);
	}

	//Light
	{
		auto id = world.make_ID();
		auto e = world.make<Entity>(id);
		auto trans = world.make<Transform>(id);
		auto dir_light = world.make<DirLight>(id);
		auto entity_editor = world.make<EntityEditor>(id);
		entity_editor->name = "Sun Light";
	}

	editor.renderer.skybox_renderer->make_default_Skybox(world, &ctx, "Tropical_Beach_3k.hdr");

	{
		auto id = world.make_ID();
		auto e = world.make<Entity>(id);
		e->layermask |= EDITOR_LAYER;
		auto trans = world.make<Transform>(id);
		auto fly = world.make<Flyover>(id);
		auto cam = world.make<Camera>(id);
		auto entity_editor = world.make<EntityEditor>(id);
		entity_editor->name = "Camera";
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

struct EditorRendererExtension : RenderExtension {
	Editor& editor;

	EditorRendererExtension(Editor& editor) : editor(editor) {}

	void render_view(World& world, RenderCtx& ctx) override {
		editor.picking.visualize(world, editor.input, ctx);

		if (editor.selected_id != -1) {
			ID selected = editor.selected_id;
			editor.outline_selected.render(world, selected, ctx);
		}
		//editor.picking_pass.render(world, ctx);
		//render_settings.render_features.append(new DebugShaderReloadSystem());
		//render_settings.render_features.append(new PickingSystem(world));

		//world.add(new Store<EntityEditor>(100));
		//world.add(new DebugShaderReloadSystem());
		//world.add(new TerrainSystem(world, this));
		//world.add(new PickingSystem(*this));
	}

	void render(World& world, RenderCtx& ctx) override {
		MainPass& main_pass = *editor.renderer.main_pass;

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
	}
};

constexpr unsigned int max_undos = 20;
constexpr unsigned int max_redos = 10;

void push_undo(Editor& editor, std::unique_ptr<EditorAction> diff) {
	editor.undos.append(std::move(diff));
	if (editor.undos.length > max_undos) {
		editor.undos.shift(1);
	}
}

void push_redo(Editor& editor, std::unique_ptr<EditorAction> diff) {
	editor.redos.append(std::move(diff));
	if (editor.redos.length > max_redos) {
		editor.redos.shift(1);
	}
}

void Editor::submit_action(EditorAction* diff) {
	push_undo(*this, std::unique_ptr<EditorAction>(diff));
}

void on_undo(Editor& editor) {
	if (editor.undos.length > 0) {
		std::unique_ptr<EditorAction> action = editor.undos.pop();
		action->undo();

		push_redo(editor, std::move(action));
	}
}

void on_redo(Editor& editor) {
	if (editor.redos.length > 0) {
		std::unique_ptr<EditorAction> action = editor.redos.pop();
		action->redo();

		push_undo(editor, std::move(action));
	}
}

void on_set_play_mode(Editor& editor, bool playing) {
	editor.playing_game = playing;

	if (playing) { //this will not fully serialize this
		editor.copy_of_world = editor.world;
	}
	else {
		editor.world = editor.copy_of_world;
	}
}

glm::vec3 Editor::place_at_cursor() {
	RayHit hit;
	hit.position = glm::vec3();
	picking.ray_cast(world, input, hit);

	return hit.position;
}

void spawn_Model(World& world, Editor& editor, model_handle model_handle) { //todo maybe move to assetTab
	ModelAsset* model_asset = editor.asset_tab.model_handle_to_asset[model_handle.id];

	if (model_asset) {

		ID id = world.make_ID();
		Entity* e = world.make<Entity>(id);

		EntityEditor* name = world.make<EntityEditor>(id);
		name->name = model_asset->name.view();

		Transform * trans = world.make<Transform>(id);

		Materials* materials = world.make<Materials>(id);
		materials->materials = model_asset->materials;

		ModelRenderer* mr = world.make<ModelRenderer>(id);
		mr->model_id = model_asset->handle;

		ID cam = get_camera(world, EDITOR_LAYER);

		Transform* cam_trans = world.by_id<Transform>(cam);
		trans->position = editor.place_at_cursor();

		editor.submit_action(new CreateAction(world, id));
	}
}

void Editor::begin_imgui(Input& input) {
	ImGui_ImplOpenGL3_NewFrame();
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

void Editor::end_imgui() {
	Profile render_imgui("Render imgui");

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	render_imgui.end();
}

void render_Editor(Editor& editor, RenderCtx& ctx) {
	Input& input = editor.input;
	Assets& assets = editor.asset_manager;
	
	editor.begin_imgui(input);
	
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::SetNextWindowBgAlpha(0.0f);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	bool p_open;

	
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
		if (ImGui::MenuItem("Undo", "CTRL+Z")) on_undo(editor);
		if (ImGui::MenuItem("Redo", "CTRL+R")) on_redo(editor);

		ImGui::MenuItem("Copy", "CTRL+C");
		ImGui::MenuItem("Paste", "CTRL+V");
		ImGui::MenuItem("Duplicate", "CTRL+D");
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Settings")) {
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();

	ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
	ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	ImGui::End();	


	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	World& world = get_World(editor);

	if (ImGui::Begin("Scene", NULL, ImGuiWindowFlags_NoScrollbar)) {
		
		//if (ImGui::ImageButton(assets, editor.get_icon("play"), ImVec2(40, 40))) {
		//	on_set_play_mode(editor, true);
		//}
		ImGui::SameLine();
		ImGui::Checkbox("Game Fullscreen", &editor.game_fullscreen);

		float width = ImGui::GetContentRegionMax().x;
		float height = ImGui::GetContentRegionMax().y - ImGui::GetCursorPos().y;

		input.region_min.x = ImGui::GetWindowPos().x + ImGui::GetCursorPos().x;
		input.region_min.y = ImGui::GetWindowPos().y + ImGui::GetCursorPos().y;
		input.region_max =  input.region_min + glm::vec2(width, height);

		ImGui::Image(assets, editor.scene_view, ImVec2(width, height));

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_AND_DROP_MODEL")) {
				spawn_Model(world, editor, *(model_handle*)payload->Data);
			}
			ImGui::EndDragDropTarget();
		}

		editor.viewport_width = width;
		editor.viewport_height = height;

		is_scene_hovered = ImGui::IsItemHovered();

		editor.gizmo.render(world, editor, ctx, input);
	}

	ImGui::End();

	//==========================

	Profile render_editor("Render Editor");

	editor.display_components.render(world, ctx, editor);
	editor.lister.render(world, editor, ctx);

	editor.asset_tab.render(world, editor, ctx);
	editor.shader_editor.render(world, editor, ctx, editor.input);
	editor.profiler.render(world, editor, ctx);

	ImGui::PopFont();
	ImGui::EndFrame();

	editor.end_imgui();

	render_editor.end();
}

Editor::~Editor() {
	log("destructor");
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void delete_object(Editor& editor) {
	World& world = get_World(editor);

	if (editor.selected_id >= 0) {
		editor.submit_action(new DestroyAction(world, editor.selected_id));
		editor.selected_id = -1;
	}
}

void mouse_click_select(Editor& editor) {
	World& world = get_World(editor);
	Input& input = editor.input;
	
	int selected = editor.picking.pick(world, input);
	editor.select(selected);
}

void respond_to_shortcut(Editor& editor) {
	Input& input = editor.input;
	World& world = get_World(editor);

	if (input.key_pressed(GLFW_KEY_P)) {
		on_set_play_mode(editor, !editor.playing_game);
	}

	if (editor.game_fullscreen && editor.playing_game) return;

	if (input.key_pressed(GLFW_KEY_X)) delete_object(editor);
	if (input.mouse_button_pressed(MouseButton::Left) && !ImGuizmo::IsOver()) mouse_click_select(editor);
	if (input.key_pressed(89, true) && input.key_down(GLFW_KEY_LEFT_CONTROL, true)) on_undo(editor);
	if (input.key_pressed('R', true) && input.key_down(GLFW_KEY_LEFT_CONTROL, true)) on_redo(editor);
	if (input.key_pressed('S', true) && input.key_down(GLFW_KEY_LEFT_CONTROL, true)) on_save(editor);

	UpdateCtx ctx(editor.time, editor.input);
	editor.gizmo.update(world, editor, ctx);
}

#include "graphics/rhi/rhi.h"

//Application
APPLICATION_API Editor* init(const char* args, Modules& modules) {
	begin_gpu_upload(*modules.rhi);
	Editor* editor = new Editor(modules, args);
	end_gpu_upload(*modules.rhi);
	return editor;
}

APPLICATION_API bool is_running(Editor* editor, Modules& engine) {
	return !engine.window->should_close() && !editor->exit;
}

APPLICATION_API void update(Editor& editor, Modules& modules) {
	respond_to_shortcut(editor);

	UpdateCtx update_ctx(editor.time, editor.input);
	update_ctx.layermask = editor.playing_game ? GAME_LAYER : EDITOR_LAYER;

	if (editor.playing_game) {
		if (update_ctx.input.key_down('R')) editor.game.reload();
		editor.game.update();
	}
	else {
		editor.fly_over_system.update(editor.world, update_ctx);
		//FlyOverSystem::update(engine.world, update_ctx);
	}
}

#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/transforms.h"

APPLICATION_API void render(Editor& editor, Modules& engine) {
	World& world = get_World(editor);

	EditorRendererExtension ext(editor);

	Layermask layermask = editor.playing_game ? GAME_LAYER : GAME_LAYER | EDITOR_LAYER;

	editor.viewport_width = engine.window->width;
	editor.viewport_height = engine.window->height;

	RenderCtx ctx = engine.renderer->render(world, layermask, editor.viewport_width, editor.viewport_height, &ext);
	//render_Editor(editor, ctx);
}

APPLICATION_API void deinit(Editor* editor) {
	delete editor;
}