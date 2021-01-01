#include "visualizer.h"
#include "mesh.h"
#include "components.h"
#include "graphics/assets/model.h"
#include "graphics/assets/material.h"
#include "graphics/assets/shader.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/primitives.h"
#include "graphics/rhi/pipeline.h"
#include "graphics/rhi/rhi.h"
#include "graphics/renderer/renderer.h"
#include "graphics/rhi/draw.h"
#include "graphics/pass/pass.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "core/time.h"
#include "cfd_ids.h"

struct CFDVisualization {
	VertexBuffer line_vertex_buffer;
	VertexBuffer solid_vertex_buffer;
	pipeline_handle pipeline_triangle_solid;
	pipeline_handle pipeline_triangle_wireframe;
	pipeline_handle pipeline_line;
};

CFDVisualization* make_cfd_visualization() {
	CFDVisualization* visualization = PERMANENT_ALLOC(CFDVisualization);
	
	shader_flags flags = 0;
	shader_handle shader = load_Shader("shaders/CFD/color.vert", "shaders/CFD/color.frag", flags);

	GraphicsPipelineDesc pipeline_desc;
	pipeline_desc.instance_layout = INSTANCE_LAYOUT_NONE;
	pipeline_desc.shader = shader;
	pipeline_desc.shader_flags = flags;
	pipeline_desc.subpass = 1;
	pipeline_desc.render_pass = RenderPass::Scene;
	pipeline_desc.range[PushConstant_Vertex].size = sizeof(glm::mat4);
	pipeline_desc.range[PushConstant_Fragment].size = 2*sizeof(glm::vec4);
	pipeline_desc.range[PushConstant_Fragment].offset = sizeof(glm::mat4);
	
	visualization->pipeline_triangle_solid = query_Pipeline(pipeline_desc);

	pipeline_desc.state = Cull_None | PolyMode_Wireframe | (3 << WireframeLineWidth_Offset);
	visualization->pipeline_triangle_wireframe = query_Pipeline(pipeline_desc);

	pipeline_desc.state = Cull_None | PrimitiveType_LineList | (3 << WireframeLineWidth_Offset);
	visualization->pipeline_line = query_Pipeline(pipeline_desc);

	return visualization;
}

void build_vertex_representation(CFDVisualization& visualization, CFDVolume& mesh) {
	vector<Vertex> line_vertices = {};
	vector<Vertex> solid_vertices = {};
	vector<uint> line_indices = {};
	vector<uint> solid_indices = {};

	line_vertices.resize(mesh.vertices.length);
	for (uint i = 0; i < mesh.vertices.length; i++) {
		Vertex& vertex = line_vertices[i];
		vertex.position = mesh.vertices[i].position;
	}

	for (uint i = 0; i < mesh.cells.length; i++) {
		const CFDCell& cell = mesh.cells[i];
		const ShapeDesc& desc = shapes[cell.type];
		for (uint i = 0; i < desc.num_faces; i++) {
			const ShapeDesc::Face& face = desc.faces[i];
			uint offset = solid_vertices.length;
			vec3 face_normal = cell.faces[i].normal;

			for (uint j = 0; j < face.num_verts; j++) {
				vertex_handle a = cell.vertices[face.verts[j]];
				vertex_handle b = cell.vertices[face.verts[(j + 1) % face.num_verts]];

				vec3 va = line_vertices[a.id].position;
				vec3 vb = line_vertices[b.id].position;

				line_indices.append(a.id);
				line_indices.append(b.id);

				Vertex vertex;
				vertex.position = va;
				vertex.normal = face_normal;
				solid_vertices.append(vertex);
			}

			solid_indices.append(offset);
			solid_indices.append(offset + 1);
			solid_indices.append(offset + 2);

			if (face.num_verts == 4) {
				solid_indices.append(offset);
				solid_indices.append(offset + 2);
				solid_indices.append(offset + 3);
			}
		}
	}

	//todo leaks memory
	begin_gpu_upload();
	visualization.line_vertex_buffer = alloc_vertex_buffer<Vertex>(VERTEX_LAYOUT_DEFAULT, line_vertices, line_indices);
	visualization.solid_vertex_buffer = alloc_vertex_buffer<Vertex>(VERTEX_LAYOUT_DEFAULT, solid_vertices, solid_indices);
	end_gpu_upload();
}

void render_cfd_mesh(CFDVisualization& visualization, CommandBuffer& cmd_buffer) {
	glm::mat4 mat(1.0);
	glm::vec4 color(1.0, 0.0, 0.0, 1.0);
	glm::vec4 line_color(1.0, 0.0, 0.0, 1.0);
	glm::vec4 cross_section_plane(1, 0, 0, -FLT_EPSILON);

	bind_pipeline(cmd_buffer, visualization.pipeline_triangle_solid);
	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_NONE);
	push_constant(cmd_buffer, VERTEX_STAGE, 0, &mat);
	push_constant(cmd_buffer, FRAGMENT_STAGE, sizeof(glm::mat4), &color);
	push_constant(cmd_buffer, FRAGMENT_STAGE, sizeof(glm::mat4) + sizeof(glm::vec4), &cross_section_plane);

	//draw_mesh(cmd_buffer, visualization.solid_vertex_buffer);

	bind_pipeline(cmd_buffer, visualization.pipeline_line);
	push_constant(cmd_buffer, FRAGMENT_STAGE, sizeof(glm::mat4), &line_color);
	draw_mesh(cmd_buffer, visualization.line_vertex_buffer);
}

RenderPass begin_cfd_scene_pass(CFDVisualization& visualization, Renderer& renderer, FrameData& frame) {
	uint frame_index = get_frame_index();
	
	SimulationUBO simulation_ubo = {};
	simulation_ubo.time = Time::now();
	memcpy_ubo_buffer(renderer.scene_pass_ubo[frame_index], &frame.pass_ubo);
	memcpy_ubo_buffer(renderer.simulation_ubo[frame_index], &simulation_ubo);

	RenderPass render_pass = begin_render_pass(RenderPass::Scene);
	next_subpass(render_pass);
	
	bind_pipeline(render_pass.cmd_buffer, visualization.pipeline_triangle_solid);
	bind_descriptor(render_pass.cmd_buffer, 0, renderer.scene_pass_descriptor[frame_index]);

	return render_pass;
}

void extract_cfd_scene_render_data(CFDSceneRenderData& render_data, World& world, EntityQuery query) {
	for (auto [e, trans, mesh] : world.filter<Transform, CFDMesh>(query)) {
		Model* model = get_Model(mesh.model);
		if (!model) continue;

		glm::mat4 mat = compute_model_matrix(trans);
		glm::vec4 color = mesh.color;

		for (Mesh& mesh : model->meshes) {
			CFDMeshInstance instance;
			instance.model = mat;
			instance.vertex = mesh.buffer[0];
			instance.color = color;

			render_data.instances.append(instance);
		}
	}
}

void render_cfd_scene(CFDVisualization& visualization, const CFDSceneRenderData render_data, CommandBuffer& cmd_buffer) {
	bind_pipeline(cmd_buffer, visualization.pipeline_triangle_solid);
	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_NONE);

	glm::vec4 cross_section_plane(0.0);
	push_constant(cmd_buffer, FRAGMENT_STAGE, sizeof(glm::mat4) + sizeof(glm::vec4), &cross_section_plane);

	for (const CFDMeshInstance& instance : render_data.instances) {
		push_constant(cmd_buffer, VERTEX_STAGE, 0, &instance.model);
		push_constant(cmd_buffer, FRAGMENT_STAGE, sizeof(glm::mat4), &instance.color);
		draw_mesh(cmd_buffer, instance.vertex);
	}

	glm::vec4 line_color(0,0,0,1.0);
	bind_pipeline(cmd_buffer, visualization.pipeline_triangle_wireframe);
	push_constant(cmd_buffer, FRAGMENT_STAGE, sizeof(glm::mat4), &line_color);

	
	for (const CFDMeshInstance& instance : render_data.instances) {
		push_constant(cmd_buffer, VERTEX_STAGE, 0, &instance.model);
		draw_mesh(cmd_buffer, instance.vertex);
	}
}
