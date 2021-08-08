#include <glm/mat4x4.hpp>
#include "visualization/input_mesh_viewer.h"
#include "engine/input.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/buffer.h"
#include "graphics/rhi/pipeline.h"
#include "components/transform.h"
#include "cfd_components.h"
#include "cfd_ids.h"

#include "ecs/ecs.h"
#include "editor/viewport.h"
#include "graphics/rhi/rhi.h"
#include "cfd_core.h"
#include "mesh/surface_tet_mesh.h"

const uint DRAW_WIREFRAME = 1 << 0;

struct CFDMeshInstance {
	glm::mat4 model;
	VertexBuffer vertex;
	glm::vec4 color;
	uint flags;
};

InputMeshViewer::InputMeshViewer(World& world, CFDRenderBackend& backend, InputMeshRegistry& registry, SceneViewport& scene_viewport)
: world(world), registry(registry), viewport(scene_viewport), backend(backend) {

	shader_flags flags = 0;
	shader_handle mesh_shader = load_Shader("shaders/CFD/color.vert", "shaders/CFD/color.frag", flags);

	GraphicsPipelineDesc pipeline_desc;
	pipeline_desc.vertex_layout = VERTEX_LAYOUT_DEFAULT;
	pipeline_desc.instance_layout = INSTANCE_LAYOUT_NONE;
	pipeline_desc.shader = mesh_shader;
	pipeline_desc.shader_flags = flags;
	pipeline_desc.subpass = 1;
	pipeline_desc.render_pass = RenderPass::Scene;
	pipeline_desc.range[PushConstant_Vertex].size = sizeof(glm::mat4);
	pipeline_desc.range[PushConstant_Fragment].size = sizeof(glm::vec4);
	pipeline_desc.range[PushConstant_Fragment].offset = sizeof(glm::mat4);

	pipeline_solid = query_Pipeline(pipeline_desc);

	pipeline_desc.state = Cull_None | PolyMode_Wireframe | (3 << WireframeLineWidth_Offset);
	pipeline_wireframe = query_Pipeline(pipeline_desc);

	alloc_triangle_buffer(backend, triangles, mb(5), mb(5));
	alloc_line_buffer(backend, lines, mb(5), mb(5));
}

void InputMeshViewer::extract_render_data(EntityQuery query) {
	render_data = {};

	uint frame_index = get_frame_index();
	CFDTriangleBuffer& triangles = this->triangles[frame_index];
	CFDLineBuffer& lines = this->lines[frame_index];

	triangles.clear();
	lines.clear();
	
	for (auto [e, trans, mesh] : world.filter<Transform, CFDMesh>(query)) {
		InputModel& model = registry.get_model(mesh.model);

		glm::mat4 mat = compute_model_matrix(trans);
		glm::vec4 color = mesh.color;

		bool selected = viewport.scene_selection.is_active(e.id);
		bool wireframe = selected && viewport.mode == SceneViewport::Object;

		for (InputMesh& mesh : model.meshes) {
			CFDMeshInstance instance = {};
			instance.model = mat;
			instance.vertex = mesh.vertex_buffer;
			instance.color = color;
			instance.flags = wireframe ? DRAW_WIREFRAME : 0;

			render_data.instances.append(instance);
		}

		SurfaceTriMesh& surface = model.surface[0];
		for (edge_handle feature : model.feature_edges) {
			
			vec3 v0 = surface.position(feature);
			vec3 v1 = surface.position(surface.TRI(feature) + (surface.TRI_EDGE(feature)+1)%3);
			lines.draw_line(v0, v1, vec4(1, 0, 0, 1));
		}
	}

	if (viewport.mode == SceneViewport::Object) {
		uint lod = 0;
		
		InputModel& model = registry.get_model(viewport.mesh_selection.model);
		SurfaceTriMesh& surface = model.surface[lod];
		VertexBuffer vert = model.meshes[0].vertex_buffer;

		vec4 color = vec4(1, 1, 1, 1);

		if (viewport.mesh_selection.mode == MeshPrimitive::Triangle) {
			for (uint triangle_id : viewport.mesh_selection.get_selected()) {
				vec3 p[3];
				for (uint i = 0; i < 3; i++) {
					p[i] = surface.position(triangle_id, i);
				}
				triangles.draw_triangle(p, vec3(0,0,0), color);
			}
		}

		if (viewport.mesh_selection.mode == MeshPrimitive::Edge) {
			for (uint edge_id : viewport.mesh_selection.get_selected()) {
				uint triangle = surface.TRI(edge_id);
				uint edge = surface.TRI_EDGE(edge_id);
				
				vec3 p0 = surface.position(triangle, edge);
				vec3 p1 = surface.position(triangle, (edge+1)%3);
				
				lines.draw_line(p0, p1, color);
			}
		}
	}
}

void InputMeshViewer::render(CommandBuffer& cmd_buffer, descriptor_set_handle frame_descriptor) {
	bind_pipeline(cmd_buffer, pipeline_solid);
	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_NONE);
	bind_descriptor(cmd_buffer, 0, frame_descriptor);

	for (const CFDMeshInstance& instance : render_data.instances) {
		push_constant(cmd_buffer, VERTEX_STAGE, 0, &instance.model);
		push_constant(cmd_buffer, FRAGMENT_STAGE, sizeof(glm::mat4), &instance.color);
		draw_mesh(cmd_buffer, instance.vertex);
	}

	bind_pipeline(cmd_buffer, pipeline_wireframe);
	push_constant(cmd_buffer, FRAGMENT_STAGE, sizeof(glm::mat4), &line_color);

	for (const CFDMeshInstance& instance : render_data.instances) {
		if (!(instance.flags & DRAW_WIREFRAME)) continue;

		push_constant(cmd_buffer, VERTEX_STAGE, 0, &instance.model);
		draw_mesh(cmd_buffer, instance.vertex);
	}

	uint frame_index = get_frame_index();
	render_triangle_buffer(backend, cmd_buffer, triangles[frame_index]);
	render_line_buffer(backend, cmd_buffer, lines[frame_index]);
}

