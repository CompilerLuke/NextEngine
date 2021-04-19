#include "editor/viewport_interaction.h"
#include "ecs/ecs.h"
#include "editor/viewport.h"
#include "components/transform.h"
#include "mesh/input_mesh.h"
#include "mesh/input_mesh_bvh.h"
#include "mesh/edge_graph.h"
#include "components/camera.h"
#include "visualization/debug_renderer.h"
#include "core/job_system/job.h"

vec3 triangle_normal(vec3 v[3]);

vec3 compute_normal(SurfaceTriMesh& surface, tri_handle triangle) {
	triangle = TRI(triangle);

	vec3 positions[3];
	for (uint i = 0; i < 3; i++) positions[i] = surface.position(triangle, i);

	return triangle_normal(positions);
}

float compute_dihedral(SurfaceTriMesh& surface, edge_handle edge) {
	vec3 normal1 = compute_normal(surface, edge);
	vec3 normal2 = compute_normal(surface, surface.edges[edge]);

	return acos(dot(normal1, normal2));
}

struct SelectLoopJob {
	SurfaceTriMesh& surface;
	InputMeshSelection& selection;
	CFDDebugRenderer& debug;
	uint starting_edge;
};

void make_feature_edge(InputMeshRegistry& registry, InputMeshSelection& selection) {
	InputModel& model = registry.get_model(selection.model);
	model.feature_edges += selection.get_selected();
}

void select_edge_loop(const SelectLoopJob& job) {
	auto& surface = job.surface;
	auto& selection = job.selection;
	auto& debug = job.debug;
	auto& starting_edge = job.starting_edge;

	EdgeGraph edge_graph = build_edge_graph(surface);

	uint current_edge = starting_edge;

	uint i = 0;
	while (i++ < 10000) {
		selection.append(current_edge);

		tri_handle triangle = TRI(current_edge);
		uint edge = TRI_EDGE(current_edge);
		vertex_handle v0 = surface.indices[triangle + edge];
		vertex_handle v1 = surface.indices[triangle + (edge + 1) % 3];

		vec3 p0 = surface.positions[v0.id];
		vec3 p1 = surface.positions[v1.id];
		vec3 p01 = p0 - p1;

		auto neighbors = edge_graph.neighbors(v0);
		float best_score = 0.0f;

		uint best_edge = 0;

		float dihedral1 = compute_dihedral(surface, current_edge);
		//draw_line(debug, p0, p1, vec4(1, 0, 0, 1));
		//suspend_execution(debug);

		for (edge_handle edge : neighbors) {
			vec3 p2 = surface.position(edge, 0);
			if (p2 == p1) continue;
			vec3 p12 = p2 - p0;
			float dihedral2 = compute_dihedral(surface, edge);

			float edge_dot = dot(normalize(p01), normalize(p12));
			float dihedral_simmilarity = 1 - fabs(dihedral1 - dihedral2) / fmaxf(dihedral1, dihedral2);

			float score = edge_dot * dihedral_simmilarity;

			//draw_line(debug, p0, p2, vec4(0, score, 0, 1));
			//suspend_execution(debug);

			if (score > best_score) {
				best_edge = edge;
				best_score = score;
			}
		}

		if (best_score > 0.4 && best_edge != starting_edge && best_edge != surface.edges[starting_edge]) current_edge = best_edge;
		else break;
	}
}

void handle_scene_interactions(World& world, InputMeshRegistry& registry, SceneViewport& viewport, CFDDebugRenderer& debug) {
	Input& input = viewport.input;
	SceneSelection& selection = viewport.scene_selection;
	InputMeshSelection& mesh_selection = viewport.mesh_selection;

	if (input.key_pressed(Key::Tab)) {
		if (viewport.mode == SceneViewport::Scene && selection.active()) {
			const CFDMesh* mesh = world.by_id<CFDMesh>(selection.get_active());

			if (mesh) {
				viewport.mode = SceneViewport::Object;
				mesh_selection.model = mesh->model;
			}
		}
		else if (viewport.mode == SceneViewport::Object) { viewport.mode = SceneViewport::Scene; }
	}

	if (input.key_pressed(Key::F, ModKeys::Control) && selection.active()) {
		make_feature_edge(registry, mesh_selection);
	}

	if (input.mouse_button_pressed() && selection.active()) {
		const Transform* trans = world.by_id<Transform>(selection.get_active());
		if (!trans) return;

		glm::mat4 model = compute_model_matrix(*trans);
		InputModelBVH& bvh = registry.get_model_bvh(mesh_selection.model);
		SurfaceTriMesh& surface = registry.get_model(mesh_selection.model).surface[0];

		Ray ray = ray_from_mouse(viewport.viewport, input.mouse_position);

		InputModelBVH::RayHit hit;
		if (bvh.ray_intersect(ray, model, hit, viewport.mesh_selection.mode)) {
			if (input.key_down(Key::Left_Alt)) {
				if (!input.key_down(Key::Left_Shift)) mesh_selection.clear();
				select_edge_loop({surface, mesh_selection, debug, hit.id});
				select_edge_loop({surface, mesh_selection, debug, surface.edges[hit.id]});
			}
			else if (input.key_down(Key::Left_Shift)) mesh_selection.toggle(hit.id);
			else mesh_selection.select(hit.id);
		}
	}
}