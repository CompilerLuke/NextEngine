#include "stdafx.h"
#include "editor/gizmo.h"
#include "ecs/ecs.h"
#include "editor/editor.h"
#include "core/input.h"
#include "GLFW/glfw3.h"
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include "components/transform.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>

void Gizmo::update(World& world, Editor& editor, UpdateParams& params) {
	if (editor.selected_id == -1) {
		this->mode = GizmoMode::DisabledGizmo;
		return;
	}
	
	if (params.input.key_pressed(GLFW_KEY_T)) {
		this->mode = GizmoMode::TranslateGizmo;
	}
	else if (params.input.key_pressed(GLFW_KEY_E)) {
		this->mode = GizmoMode::ScaleGizmo;
	}
	else if (params.input.key_pressed(GLFW_KEY_R)) {
		this->mode = GizmoMode::RotateGizmo;
	}
	else if (params.input.key_down(GLFW_KEY_ESCAPE)) {
		this->mode = GizmoMode::DisabledGizmo;
	}
	else if (params.input.key_down(GLFW_KEY_LEFT_CONTROL)) {
		this->snap = true;
	} 
	else {
		this->snap = false;
	}
}

Gizmo::~Gizmo() {
	delete this->diff_util;
}

void Gizmo::render(World& world, Editor& editor, RenderParams& params) {
	if (editor.selected_id == -1) return;

	Transform* trans = world.by_id<Transform>(editor.selected_id);

	ImGuizmo::OPERATION guizmo_operation;
	ImGuizmo::MODE guizmo_mode = ImGuizmo::WORLD;

	if (mode == TranslateGizmo) guizmo_operation = ImGuizmo::TRANSLATE;
	else if (mode == RotateGizmo) guizmo_operation = ImGuizmo::ROTATE;
	else if (mode == ScaleGizmo) guizmo_operation = ImGuizmo::SCALE;
	else return;

	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

	glm::mat4 model_matrix = trans->compute_model_matrix();

	glm::vec3 snap_vec(snap_amount);

	ImGuizmo::BeginFrame();
	ImGuizmo::Enable(true);

	bool was_using = ImGuizmo::IsUsing();

	if (diff_util == NULL) {
		diff_util = new DiffUtil(trans, &default_allocator);
	}

	ImGuizmo::Manipulate(glm::value_ptr(params.view), glm::value_ptr(params.projection), guizmo_operation, guizmo_mode, glm::value_ptr(model_matrix), NULL, snap ? glm::value_ptr(snap_vec) : NULL);

	if (was_using != ImGuizmo::IsUsing()) {
		if (diff_util->submit(editor, "Transformed")) {
			delete diff_util;
			diff_util = new DiffUtil(trans, &default_allocator);
		}
	}

	glm::mat4 transformation = model_matrix; // your transformation matrix.
	glm::vec3 scale;
	glm::quat rotation;
	glm::vec3 translation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(transformation, scale, rotation, translation, skew, perspective);

	if (mode == TranslateGizmo) trans->position = translation;
	if (mode == RotateGizmo) trans->rotation = glm::conjugate(rotation);
	if (mode == ScaleGizmo) trans->scale = scale;
}