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
#include "graphics/renderer/material_system.h"
#include "core/serializer.h"
#include "terrain.h"
#include <ImGuizmo/ImGuizmo.h>
#include "core/profiler.h"
#include "graphics/renderer/renderer.h"
#include "engine/engine.h"
#include "graphics/assets/asset_manager.h"

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
	return editor.engine.world;
}

string_buffer save_component_to(ComponentStore* store) {
	return string_buffer("data/") + store->get_component_type()->name + ".ne";
}

void on_load_world(Editor& editor) {
	Level& level = editor.engine.level;
	World& world = get_World(editor);

	unsigned int save_files_available = 0;

	for (int i = 0; i < world.components_hash_size; i++) {
		auto store = world.components[i].get();
		if (store != NULL) {
			File file(level);
			if (!file.open(save_component_to(store).view(), File::ReadFileB)) continue;

			char* data_buffer = NULL;
			unsigned int data_length = file.read_binary(&data_buffer);

			DeserializerBuffer buffer(data_buffer, data_length);
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

	//if (save_files_available == 0) default_scene(editor, params);
}

void on_load(Editor& editor, RenderCtx& ctx) {
	World& world = get_World(editor);

	on_load_world(editor);

	world.get<Skybox>()->fire_callbacks();
	ctx.skybox = world.filter<Skybox>()[0];

	editor.asset_tab.on_load(world, ctx);
}

void on_save_world(Editor& editor) {
	World& world = get_World(editor);
	Level& level = editor.engine.asset_manager.level;

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

			File file(level);

			if (!file.open(save_component_to(store).view(), File::WriteFileB)) throw "Could not save data";
			file.write({ buffer.data, buffer.index });
		}
	}
}

void on_save(Editor& editor) {
	on_save_world(editor);
	editor.asset_tab.on_save();
}

void register_callbacks(Editor& editor, Engine& engine) {
	Window& window = engine.window;
	World& world = engine.world;
	
	editor.selected.listeners.clear();

	register_on_inspect_callbacks(engine.asset_manager);

	editor.asset_tab.register_callbacks(window, editor);

	editor.selected.listen([&engine, &world](ID id) {
		auto rb = world.by_id<RigidBody>(id);
		if (rb) {
			rb->bt_rigid_body = NULL; //todo fix leak
		}
	});

	engine.window.override_key_callback(override_key_callback);
	engine.window.override_char_callback(ImGui_ImplGlfw_CharCallback);
	engine.window.override_mouse_button_callback(override_mouse_button_callback);
}

Editor::Editor(Engine& engine, const char* game_code) : 
	engine(engine), 
	picking_pass(engine.asset_manager, glm::vec2(engine.window.width, engine.window.height), engine.renderer.main_pass), 
	game(engine, game_code),
	asset_tab(engine.renderer, engine.asset_manager, engine.window)
{
	Window& window = engine.window;
	World& world = engine.world;


	//world.add(new DebugShaderReloadSystem());
	//world.add(new TerrainSystem(world, this));
	//world.add(new PickingSystem(*this));

	init_imgui();
	register_callbacks(*this, engine);

	AttachmentDesc color_attachment(scene_view);
	color_attachment.min_filter = Filter::Linear;
	color_attachment.mag_filter = Filter::Linear;

	FramebufferDesc settings;
	settings.width = window.width;
	settings.height = window.height;
	settings.color_attachments.append(color_attachment);

	scene_view_fbo = Framebuffer(engine.asset_manager.textures, settings);

	Time time;

	TextureManager& texture_manager = engine.asset_manager.textures;

	icons = {
		{ "play",   texture_manager.load("editor/play_button3.png") },
		{ "folder", texture_manager.load("editor/folder_icon.png") },
		{ "shader", texture_manager.load("editor/shader-works-icon.png") }
	};

	MainPass* main_pass = engine.renderer.main_pass;
	main_pass->post_process.append(&picking_pass);

	CommandBuffer cmd_buffer(engine.asset_manager);
	RenderCtx render_ctx(cmd_buffer, main_pass);
	render_ctx.layermask = GAME_LAYER | EDITOR_LAYER;
	render_ctx.width = window.width;
	render_ctx.height = window.height;

	on_load(*this, render_ctx); //todo remove any rendering from load

	engine.input.capture_mouse(false);

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

	auto font_path = engine.level.asset_path("fonts/segoeui.ttf");
	ImFont* font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 32.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());

	io.FontAllowUserScaling = true;

	asset_tab.filename_font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 26.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());
	asset_tab.default_font = font;

	for (int i = 0; i < 10; i++) {
		ImFontConfig im_font_config; im_font_config.OversampleH = 5; im_font_config.OversampleV = 5;

		shader_editor.font[i] = io.Fonts->AddFontFromFileTTF(font_path.c_str(), (i + 1) * 10.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());
	}

	set_darcula_theme();

	GLFWwindow* window_ptr = engine.window.window_ptr;

	ImGui_ImplGlfw_InitForOpenGL(window_ptr, false);
	ImGui_ImplOpenGL3_Init();

}

uint64_t Editor::get_icon(string_view name) {
	TextureManager& textures = engine.asset_manager.textures;
	
	for (auto& icon : icons) {
		if (icon.name == name) {
			return gl_id_of(textures, icon.texture_id);
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
	AssetManager& assets = editor.engine.asset_manager;

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
	model_render->model_id = assets.models.load("HOVERTANK.fbx");

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
		mr->model_id = assets.models.load("subdivided_plane8.fbx");

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

	/*
	{
		ID id = editor.asset_tab.assets.make_ID();
		MaterialAsset* asset = editor.asset_tab.assets.make<MaterialAsset>(id);
		asset->name = "Skybox Material";
		asset->handle = world.by_id<Materials>(world.id_of(skybox))->materials[0];

		register_new_material(world, asset_tab, *this, render_ctx, id);
	}
	*/
}

float fullscreen = false;

struct EditorRendererExtension : RenderExtension {
	Editor& editor;

	EditorRendererExtension(Editor& editor) : editor(editor) {}

	void render_view(World& world, RenderCtx& ctx) override {
		//editor.picking_pass.render(world, ctx);
		//render_settings.render_features.append(new DebugShaderReloadSystem());
		//render_settings.render_features.append(new PickingSystem(world));

		//world.add(new Store<EntityEditor>(100));
		//world.add(new DebugShaderReloadSystem());
		//world.add(new TerrainSystem(world, this));
		//world.add(new PickingSystem(*this));
	}

	void render(World& world, RenderCtx& ctx) override {
		MainPass& main_pass = *editor.engine.renderer.main_pass;

		if (fullscreen && editor.playing_game) {
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
		editor.game.reload();
		on_save_world(editor);
	}
	else {
		get_World(editor).clear(); //todo leaks memory
		on_load_world(editor);
	}
}

glm::vec3 Editor::place_at_cursor() {
	World& world = get_World(*this);
	Input& input = engine.input;

	ID cam = get_camera(world, EDITOR_LAYER);
	
	glm::mat4 view_m = get_view_matrix(world, cam);
	glm::mat4 proj_m = get_proj_matrix(world, cam, viewport_width / viewport_height);

	glm::mat4 to_world = glm::inverse(proj_m * view_m);

	float height = input.region_max.y - input.region_min.y;
	float width = input.region_max.x - input.region_min.x;

	glm::vec4 pos = glm::vec4(input.mouse_position.x / width, 1.0 - (input.mouse_position.y / height), picking_pass.pick_depth(world, input), 1.0);

	pos *= 2.0;
	pos -= 1.0;

	pos = to_world * pos;
	pos /= pos.w;

	return pos;
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
	Input& input = editor.engine.input;
	TextureManager& textures = editor.engine.asset_manager.textures;
	
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
		
		if (ImGui::ImageButton((ImTextureID)editor.get_icon("play"), ImVec2(40, 40))) {
			on_set_play_mode(editor, true);
		}

		float width = ImGui::GetContentRegionMax().x;
		float height = ImGui::GetContentRegionMax().y - ImGui::GetCursorPos().y;

		input.region_min.x = ImGui::GetWindowPos().x + ImGui::GetCursorPos().x;
		input.region_min.y = ImGui::GetWindowPos().y + ImGui::GetCursorPos().y;
		input.region_max =  input.region_min + glm::vec2(width, height);

		ImGui::Image((ImTextureID)gl_id_of(textures, editor.scene_view), ImVec2(width, height), ImVec2(0, 1), ImVec2(1, 0));

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
	editor.shader_editor.render(world, editor, ctx, editor.engine.input);
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
	editor.select(editor.picking_pass.pick(get_World(editor), editor.engine.input));
}

void respond_to_shortcut(Editor& editor) {
	Input& input = editor.engine.input;
	World& world = get_World(editor);

	if (input.key_pressed(GLFW_KEY_P)) {
		on_set_play_mode(editor, !editor.playing_game);
	}

	if (editor.playing_game) return;

	if (input.key_pressed(GLFW_KEY_X)) delete_object(editor);
	if (input.mouse_button_pressed(MouseButton::Left) && !ImGuizmo::IsOver()) mouse_click_select(editor);
	if (input.key_pressed(89, true) && input.key_down(GLFW_KEY_LEFT_CONTROL, true)) on_undo(editor);
	if (input.key_pressed('R', true) && input.key_down(GLFW_KEY_LEFT_CONTROL, true)) on_redo(editor);
	if (input.key_pressed('S', true) && input.key_down(GLFW_KEY_LEFT_CONTROL, true)) on_save(editor);

	UpdateCtx ctx(editor.engine.time, editor.engine.input);
	editor.gizmo.update(world, editor, ctx);
}

//Application
APPLICATION_API Editor* init(const char* args, Engine& engine) {
	return new Editor(engine, args);
}

APPLICATION_API bool is_running(Editor* editor, Engine& engine) {
	return !engine.window.should_close() && !editor->exit;
}

APPLICATION_API void update(Editor& editor, Engine& engine) {
	respond_to_shortcut(editor);

	UpdateCtx update_ctx(engine.time, editor.engine.input);
	update_ctx.layermask = editor.playing_game ? GAME_LAYER : EDITOR_LAYER;

	if (editor.playing_game) editor.game.update();
	else {
		editor.fly_over_system.update(engine.world, update_ctx);
		//FlyOverSystem::update(engine.world, update_ctx);
	}
}

#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/transforms.h"

APPLICATION_API void render(Editor& editor, Engine& engine) {
	World& world = get_World(editor);

	EditorRendererExtension ext(editor);

	Layermask layermask = editor.playing_game ? GAME_LAYER : GAME_LAYER | EDITOR_LAYER;

	RenderCtx ctx = engine.renderer.render(world, layermask, editor.viewport_width, editor.viewport_height, &ext);
	render_Editor(editor, ctx);
}

APPLICATION_API void deinit(Editor* editor) {
	delete editor;
}