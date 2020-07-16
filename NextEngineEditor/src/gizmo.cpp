#include "gizmo.h"
#include "ecs/ecs.h"
#include "editor.h"
#include "core/io/input.h"
#include "graphics/renderer/renderer.h"
#include "GLFW/glfw3.h"
#include <imgui/imgui.h>
#include <ImGuizmo/ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include "components/transform.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <core/types.h>

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
	
	if (params.input.key_down(GLFW_KEY_T)) {
		this->mode = GizmoMode::Translate;
	}
	else if (params.input.key_down(GLFW_KEY_E)) {
		this->mode = GizmoMode::Scale;
	}
	else if (params.input.key_down(GLFW_KEY_R)) {
		this->mode = GizmoMode::Rotate;
	}
	else if (params.input.key_down(GLFW_KEY_ESCAPE)) {
		this->mode = GizmoMode::Disabled;
	}
	else if (params.input.key_down(GLFW_KEY_LEFT_CONTROL, true)) {
		this->snap = true;
	} 
	else {
		this->snap = false;
	}
}

Gizmo::~Gizmo() {}

void Gizmo::render(World& world, Editor& editor, Viewport& viewport, Input& input) {
	if (editor.selected_id == -1) return;

	Transform* trans = world.by_id<Transform>(editor.selected_id);
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

	if (!was_using && is_using) begin_diff(diff_util, trans, &copy_transform, get_Transform_type());

	if (mode == GizmoMode::Translate) trans->position = translation;
	if (mode == GizmoMode::Rotate) trans->rotation = glm::conjugate(rotation);
	if (mode == GizmoMode::Scale) trans->scale = scale;

	if (was_using && !is_using) {
		char desc[50];
		sprintf_s(desc, "Moved %i", editor.selected_id);

		end_diff(editor.actions, diff_util, desc);
	}
	else if (was_using) {
		commit_diff(editor.actions, diff_util);
	}
}