#include "gizmo.h"
#include "ecs/ecs.h"
#include "editor.h"
#include "engine/input.h"
#include "graphics/renderer/renderer.h"
#include <imgui/imgui.h>
#include <ImGuizmo/ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include "components/transform.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "core/types.h"
#include "graphics/assets/assets.h"

material_handle make_icon_material(string_view path, bool billboard = true) {
	MaterialDesc mat_desc{ load_Shader(billboard ? "shaders/icon.vert" : "shaders/pbr.vert", "shaders/icon.frag") };
	mat_desc.draw_state = Cull_None;

	Image image = load_Image(path, true);
	mat_channel4(mat_desc, "icon", glm::vec4(1), upload_Texture(image));
	free_Image(image);

	return make_Material(mat_desc);
}

void make_special_gizmo_resources(GizmoResources& resources) {
	{
		MaterialDesc mat_desc{ load_Shader("shaders/pbr.vert", "shaders/gizmo.frag") };
		mat_desc.draw_state = DepthMask_None;
		mat_vec3(mat_desc, "color", glm::vec3(1, 1, 0));

		resources.terrain_control_point_material = make_Material(mat_desc);
	}

	{
		MaterialDesc mat_desc{ load_Shader("shaders/pbr.vert", "shaders/diffuse.frag") };
		mat_channel3(mat_desc, "diffuse", glm::vec3(0,0,1));

		resources.camera_material = make_Material(mat_desc);
	}

	resources.terrain_splat_material = make_icon_material("editor/brush_icon.png");
	resources.point_material = make_icon_material("editor/point_light_icon.png");
	resources.dir_light_material = make_icon_material("editor/dir_light_icon.png");
	resources.grass_system_material = make_icon_material("editor/grass_system_icon.png");

	resources.camera_model = load_Model("editor/camera.fbx");
	//resources.particle_system_material = make_icon_material("editor/particle_system_icon.png");
}

void extract_special_gizmo_transform(tvector<glm::mat4>& matrices, World& world, glm::vec3 scale, EntityQuery query, bool billboard = true) {
	for (auto[e, orig_trans] : world.filter<Transform>(query)) {
		Transform trans;
		trans.scale = scale;
		trans.position = orig_trans.position;
		if (!billboard) trans.rotation = orig_trans.rotation;

		glm::mat4 model_m = compute_model_matrix(trans);
		matrices.append(model_m);
	}
}

void extract_render_data_special_gizmo(GizmoRenderData& render_data, World& world, EntityQuery query) {
	extract_special_gizmo_transform(render_data.terrain_control_points, world, glm::vec3(0.5), query.with_all<TerrainControlPoint>());
	extract_special_gizmo_transform(render_data.terrain_splats, world, glm::vec3(0.2), query.with_all<TerrainSplat>());
	extract_special_gizmo_transform(render_data.cameras, world, glm::vec3(0.2), query.with_all<Camera>().with_none(EDITOR_ONLY), false);
	extract_special_gizmo_transform(render_data.point_lights, world, glm::vec3(1.0), query.with_all<PointLight>());
	extract_special_gizmo_transform(render_data.dir_lights, world, glm::vec3(5.0), query.with_all<DirLight>());
	extract_special_gizmo_transform(render_data.grass_system, world, glm::vec3(0.2), query.with_all<Grass>());
}

void render_special_gizmos(GizmoResources& resources, const GizmoRenderData& render_data, RenderPass& render_pass) {
	CommandBuffer& cmd_buffer = render_pass.cmd_buffer;
	
	draw_mesh(cmd_buffer, resources.camera_model, resources.camera_material, render_data.cameras);
	draw_mesh(cmd_buffer, primitives.quad, resources.terrain_splat_material, render_data.terrain_splats);
	draw_mesh(cmd_buffer, primitives.quad, resources.dir_light_material, render_data.dir_lights);
	draw_mesh(cmd_buffer, primitives.quad, resources.point_material, render_data.point_lights);	
	draw_mesh(cmd_buffer, primitives.quad, resources.grass_system_material, render_data.grass_system);
	draw_mesh(cmd_buffer, primitives.sphere, resources.terrain_control_point_material, render_data.terrain_control_points);
}

void Gizmo::register_callbacks(Editor& editor) {
	editor.selected.listen([this](ID id) {
		this->diff_util.copy_ptr = NULL;
	});
}

void Gizmo::update(World& world, Editor& editor, UpdateCtx& params) {
	if (editor.selected_id == -1) {
		this->mode = GizmoMode::Disabled;
		return;
	}
	
	if (params.input.key_down(Key::T)) {
		this->mode = GizmoMode::Translate;
	}
	else if (params.input.key_down(Key::E)) {
		this->mode = GizmoMode::Scale;
	}
	else if (params.input.key_down(Key::R)) {
		this->mode = GizmoMode::Rotate;
	}
	else if (params.input.key_down(Key::Escape)) {
		this->mode = GizmoMode::Disabled;
	}
	else if (params.input.key_down(Key::Left_Control)) {
		this->snap = true;
	} 
	else {
		this->snap = false;
	}
}

Gizmo::~Gizmo() {}

void Gizmo::render(World& world, Editor& editor, Viewport& viewport, Input& input) {
	if (editor.selected_id == -1) return;

	Transform* trans = world.m_by_id<Transform>(editor.selected_id);
	if (trans == nullptr) return;

	ImGuizmo::OPERATION guizmo_operation;
	ImGuizmo::MODE guizmo_mode = ImGuizmo::WORLD;

	if (mode == GizmoMode::Translate) guizmo_operation = ImGuizmo::TRANSLATE;
	else if (mode == GizmoMode::Rotate) guizmo_operation = ImGuizmo::ROTATE;
	else if (mode == GizmoMode::Scale) guizmo_operation = ImGuizmo::SCALE;
	else return;

	ImGuiIO& io = ImGui::GetIO();

	glm::mat4 model_matrix = compute_model_matrix(*trans);

	glm::vec3 snap_vec(snap_amount);

	ImGuizmo::BeginFrame();
	ImGuizmo::Enable(true);

	bool was_using = ImGuizmo::IsUsing();


	//ImGuizmo rendering behind other screen

	ImGuizmo::SetRect(input.region_min.x, input.region_min.y, input.region_max.x - input.region_min.x, input.region_max.y - input.region_min.y);

	ImGuizmo::Manipulate(glm::value_ptr(viewport.view), glm::value_ptr(viewport.proj), guizmo_operation, guizmo_mode, glm::value_ptr(model_matrix), NULL, snap ? glm::value_ptr(snap_vec) : NULL);

    

	glm::mat4 transformation = model_matrix; // your transformation matrix.
	glm::vec3 scale;
	glm::quat rotation;
	glm::vec3 translation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(transformation, scale, rotation, translation, skew, perspective);

	bool is_using = ImGuizmo::IsUsing();

	if (!was_using && is_using) begin_e_diff<Transform>(diff_util, world, editor.selected_id, &copy_transform);

	if (mode == GizmoMode::Translate) trans->position = translation;
	if (mode == GizmoMode::Rotate) trans->rotation = glm::conjugate(rotation);
	if (mode == GizmoMode::Scale) trans->scale = scale;

	if (was_using && !is_using) {
		char desc[50];
		snprintf(desc, 50, "Moved %i", editor.selected_id);

		end_diff(editor.actions, diff_util, desc);
	}
	else if (was_using) {
		commit_diff(editor.actions, diff_util);
	}
}
