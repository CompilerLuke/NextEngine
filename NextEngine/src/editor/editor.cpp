#include "stdafx.h"
#include "editor/editor.h"
#include "graphics/window.h"
#include "core/input.h"
#include "ecs/system.h"
#include "logger/logger.h"
#include "reflection/reflection.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "ecs/ecs.h"
#include "graphics/shader.h"
#include "model/model.h"
#include "graphics/texture.h"
#include "core/game_time.h"
#include "graphics/renderPass.h"
#include "graphics/ibl.h"
#include "components/lights.h"
#include "components/transform.h"
#include "components/camera.h"
#include "components/flyover.h"
#include "graphics/rhi.h"
#include "editor/custom_inspect.h"
#include "editor/picking.h"
#include "core/vfs.h"
#include "graphics/materialSystem.h"
#include "serialization/serializer.h"
#include "editor/terrain.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

DEFINE_COMPONENT_ID(EntityEditor, 15)

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

void override_key_callback(GLFWwindow* window_ptr, int key, int scancode, int action, int mods) {
	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
	
	ImGui_ImplGlfw_KeyCallback(window_ptr, key, scancode, action, mods);

	if (!ImGui::GetIO().WantCaptureKeyboard) {
		window->on_key.broadcast({ key, scancode, action, mods });
	}
}

void override_mouse_button_callback(GLFWwindow* window_ptr, int button, int action, int mods) {
	auto window = (Window*)glfwGetWindowUserPointer(window_ptr);
	
	ImGui_ImplGlfw_MouseButtonCallback(window_ptr, button, action, mods);

	if (!ImGui::GetIO().WantCaptureMouse) {
		window->on_mouse_button.broadcast({ button, action, mods });
	}
}

void Editor::select(ID id) {
	this->selected_id = id;
}

wchar_t* to_wide_char(const char* orig) {
	unsigned int newsize = strlen(orig) + 1;
	wchar_t * wcstring = new wchar_t[newsize];
	size_t convertedChars;
	mbstowcs_s(&convertedChars, wcstring, newsize, orig, _TRUNCATE);
	return wcstring;
}

typedef void(*register_components_and_systems_t)(World&);

register_components_and_systems_t load_register_components_and_systems(const char* game_code) {
	wchar_t* game_code_w = to_wide_char(game_code);
	HINSTANCE hGetProcIDDLL = LoadLibrary(game_code_w);
	delete game_code_w;

	if (!hGetProcIDDLL) {
		log("could not load the dynamic library");

		throw EXIT_FAILURE;
	}
	// resolve function address here
	auto func = (register_components_and_systems_t)GetProcAddress(hGetProcIDDLL, "register_components_and_systems");
	if (!func) {
		log("could not locate the function");
		throw EXIT_FAILURE;
	}

	return func;
}

Editor::Editor(const char* game_code, Window& window)
: window(window), picking_pass(window), game_code(game_code), main_pass(world, window) {
	
	load_register_components_and_systems(game_code)(world);
	world.add(new Store<EntityEditor>(100));

	register_on_inspect_callbacks();
}

void Editor::init_imgui() {
	// Setup ImGui binding
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigDockingWithShift = false;

	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

	ImFontConfig icons_config; icons_config.MergeMode = false; icons_config.PixelSnapH = true; icons_config.FontDataOwnedByAtlas = true; icons_config.FontDataSize = 72;

	auto font_path = Level::asset_path("fonts/segoeui.ttf");
	ImFont* font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 32.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());
	asset_tab.filename_font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 26.0f, &icons_config, io.Fonts->GetGlyphRangesDefault());
	asset_tab.default_font = font;

	set_darcula_theme();

	ImGui_ImplGlfw_InitForOpenGL(window.window_ptr, false);
	ImGui_ImplOpenGL3_Init();

	glfwSetKeyCallback(window.window_ptr, override_key_callback);
	glfwSetCharCallback(window.window_ptr, ImGui_ImplGlfw_CharCallback);
	glfwSetMouseButtonCallback(window.window_ptr, override_mouse_button_callback);
}

ImTextureID Editor::get_icon(StringView name) {
	for (auto icon : icons) {
		if (icon.name == name) {
			return (ImTextureID) texture::id_of(icon.texture_id);
		}
	}
	
	throw "Could not find icon";
}

void default_scene(Editor& editor, RenderParams& params) {
	World& world = editor.world;

	editor.asset_tab.default_material = create_new_material(world, editor.asset_tab, editor, params)->handle;

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
	model_render->model_id = load_Model("HOVERTANK.fbx");

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
		mr->model_id = load_Model("subdivided_plane8.fbx");

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
}

StringBuffer save_component_to(ComponentStore* store) {
	return StringBuffer("data/") + store->get_component_type()->name + ".ne";
}

void on_load(Editor& editor, RenderParams& params) {
	unsigned int save_files_available = 0;

	for (int i = 0; i < editor.world.components_hash_size; i++) {
		auto store = editor.world.components[i].get();
		if (store != NULL) {
			File file;
			if (!file.open(save_component_to(store).view(), File::ReadFileB)) continue;

			char* data_buffer = NULL;
			unsigned int data_length = file.read_binary(&data_buffer);

			DeserializerBuffer buffer(data_buffer, data_length);
			reflect::TypeDescriptor* component_type = store->get_component_type();

			unsigned num_components = buffer.read_int();
			for (unsigned int i = 0; i < num_components; i++) {
				unsigned int id = buffer.read_int();
				void* ptr = store->make_by_id(id);
			
				buffer.read(component_type, ptr);
				editor.world.skipped_ids.append(id);
			}

			save_files_available++;
		}
	}

	editor.asset_tab.on_load(editor.world, params);

	if (save_files_available == 0) default_scene(editor, params);

}

void on_save(Editor& editor) {
	for (int i = 0; i < editor.world.components_hash_size; i++) {
		auto store = editor.world.components[i].get();
		if (store != NULL) {
			SerializerBuffer buffer;

			vector<Component> filtered;
			for (Component& comp : store->filter_untyped()) {
				if (editor.world.by_id<EntityEditor>(comp.id))
					filtered.append(comp);
			}

			buffer.write_int(filtered.length);

			log("Serialized ", filtered.length, " ", store->get_component_type()->name);

			for (Component& comp : filtered) {
				buffer.write_int(comp.id);
				buffer.write(comp.type, comp.data);
			}

			File file;

			if (!file.open(save_component_to(store).view(), File::WriteFileB)) throw "Could not save data";
			file.write({ buffer.data, buffer.index });
		}
	}

	editor.asset_tab.on_save();
}

void Editor::run() {
	this->asset_tab.register_callbacks(window, *this);

	init_imgui();

	Input input(window);
	Time time;

	icons = {
		{"play", load_Texture("editor/play_button3.png")},
		{"folder", load_Texture("editor/folder_icon.png")},
		{"shader", load_Texture("editor/shader_icon.png")}
	};

	auto shader = load_Shader("shaders/pbr.vert", "shaders/gizmo.frag");
	auto texture = load_Texture("normal.jpg");
	auto model = load_Model("HOVERTANK.fbx");
	auto skybox = load_Skybox(world, "Topanga_Forest_B_3k.hdr");

	load_Model("subdivided_plane8.fbx");

	CommandBuffer cmd_buffer;
	RenderParams render_params(&cmd_buffer, &main_pass);

	main_pass.post_process.append(&picking_pass);

	PickingSystem* picking_system = new PickingSystem(*this);

	UpdateParams update_params(input);

	//Create grid

	vector<Param> params = make_SubstanceMaterial("wood_2", "Stylized_Wood");

	/*Material material = {
		std::string("DefaultMaterial"),
		load_Shader("shaders/pbr.vert", "shaders/pbr.frag"),
		params,
		&default_draw_state
	};

	*/

	render_params.layermask = game_layer | editor_layer;
	render_params.width = window.width;
	render_params.height = window.height;
	render_params.skybox = skybox;

	{
		auto id = world.make_ID();
		auto e = world.make<Entity>(id);
		auto trans = world.make<Transform>(id);
		auto camera = world.make <Camera>(id);
		auto flyover = world.make<Flyover>(id);
	}

	//default_scene(*this, render_params);
	
	on_load(*this, render_params);

	world.add(new DebugLightSystem());
	world.add(new DebugShaderReloadSystem());
	world.add(new TerrainSystem());
	world.add(picking_system);

	load_Shader("shaders/pbr.vert", "shaders/paralax_pbr.frag");

	render_params.dir_light = get_dir_light(world, render_params.layermask);

	while (!window.should_close() && !exit) {
		temporary_allocator.clear();
		cmd_buffer.clear();

		time.update_time(update_params);
		input.clear();
		window.poll_inputs();

		update_params.layermask = game_layer;

		render_params.layermask = game_layer | editor_layer;
		render_params.width = window.width;
		render_params.height = window.height;
		render_params.dir_light = get_dir_light(world, render_params.layermask);
		render_params.skybox = skybox;

		world.update(update_params);
		update(update_params);

		if (window.height == window.width == 0) { //screen is hidden
			render_params.pass->render(world, render_params);
			render(render_params);
		}

		window.swap_buffers();
	}

	on_save(*this);
}

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

void Editor::update(UpdateParams& params) {
	if (params.input.key_pressed(GLFW_KEY_X)) {
		if (this->selected_id >= 0) {
			submit_action(new DestroyAction(world, this->selected_id));

			this->selected_id = -1;
		}
	}

	if (params.input.key_pressed(GLFW_KEY_L)) {
		log("about to serialize");
	}

	if (params.input.mouse_button_pressed(MouseButton::Left)) {
		this->selected_id = picking_pass.pick(world, params);
	}

	if (params.input.key_pressed(89, true) && params.input.key_down(GLFW_KEY_LEFT_CONTROL, true)) on_undo(*this);
	if (params.input.key_pressed('R', true) && params.input.key_down(GLFW_KEY_LEFT_CONTROL, true)) on_redo(*this);
	if (params.input.key_pressed('S', true) && params.input.key_down(GLFW_KEY_LEFT_CONTROL, true)) on_save(*this);

	display_components.update(world, params);
	gizmo.update(world, *this, params);
	asset_tab.update(world, *this, params);
}


void Editor::render(RenderParams& params) {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	unsigned char* pixels;
	int width, height;
	ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	ImFontAtlas* atlas = ImGui::GetIO().Fonts;

	ImGui::PushFont(atlas->Fonts[0]);

	
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
			on_save(*this);
		}
		if (ImGui::MenuItem("Exit", "ALT+F4")) {
			log("Exiting");
			this->exit = true;
		}
		ImGui::EndMenu();
	}
	
	if (ImGui::BeginMenu("Edit")) {
		if (ImGui::MenuItem("Undo", "CTRL+Z")) on_undo(*this);
		if (ImGui::MenuItem("Redo", "CTRL+R")) on_redo(*this);

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

	ImGui::Begin("Game");
	ImGui::ImageButton(get_icon("play"), ImVec2(40, 40));
	ImGui::End();

	//==========================

	display_components.render(world, params, *this);
	lister.render(world, *this, params);
	gizmo.render(world, *this, params);
	asset_tab.render(world, *this, params);

	ImGui::PopFont();
	ImGui::EndFrame();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

Editor::~Editor() {
	log("destructor");
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}