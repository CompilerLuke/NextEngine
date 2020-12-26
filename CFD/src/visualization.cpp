#include "visualizer.h"
#include "mesh.h"
#include "graphics/assets/model.h"
#include "graphics/assets/material.h"
#include "graphics/assets/shader.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/primitives.h"
#include "graphics/rhi/pipeline.h"
#include "graphics/rhi/rhi.h"

void make_cfd_visualization(CFDVisualization& visualization) {
	MaterialDesc desc = {default_shaders.gizmo};
	mat_vec3(desc, "color", glm::vec3(1.0,0.0,0.0));

	desc.draw_state = Cull_None | PrimitiveType_LineList | (1 << WireframeLineWidth_Offset);

	visualization.material = make_Material(desc);
}

void build_vertex_representation(CFDVisualization& visualization, CFDVolume& mesh) {
	vector<Vertex> vertices = {};
	vector<uint> indices = {};

	vertices.resize(mesh.vertices.length);
	for (uint i = 0; i < mesh.vertices.length; i++) {
		Vertex& vertex = vertices[i];
		vertex.position = mesh.vertices[i].position;
	}

	for (uint i = 0; i < mesh.cells.length; i++) {
		const CFDCell& cell = mesh.cells[i];
		const ShapeDesc& desc = shapes[cell.type];
		for (uint i = 0; i < desc.num_faces; i++) {
			const ShapeDesc::Face& face = desc.faces[i];
			uint offset = vertices.length;
			vec3 face_normal = cell.faces[i].normal;

			for (uint j = 0; j < face.num_verts; j++) {
				vertex_handle a = cell.vertices[face.verts[j]];
				vertex_handle b = cell.vertices[face.verts[(j + 1) % face.num_verts]];

				vec3 va = vertices[a.id].position;
				vec3 vb = vertices[b.id].position;

				//printf("Drawing line %i(%f,%f,%f), %i(%f,%f,%f)\n", a.id, va.x, va.y, va.z, b.id, vb.x, vb.y, vb.z);

				indices.append(a.id);
				indices.append(b.id);

				//vertex_handle handle = cell.vertices[face.to_verts[j]];
				//CFDVertex cfd_vertex = mesh.get_vertex(handle);

				//Vertex vertex;
				//vertex.position = cfd_vertex.position;
				//vertex.normal = face_normal;
				//vertices.append(vertex);

				//indices.append(offset + j);
				//indices.append(offset + (j + 1) % face.to_verts.length);
			}
		}
	}

	//todo leaks memory
	begin_gpu_upload();
	visualization.vertex_buffer = alloc_vertex_buffer<Vertex>(VERTEX_LAYOUT_DEFAULT, vertices, indices);
	end_gpu_upload();
}

void render_cfd_mesh(CFDVisualization& visualization, CommandBuffer& cmd_buffer) {
	glm::mat4 mat(1.0);

	InstanceBuffer instance_buffer = frame_alloc_instance_buffer<glm::mat4>(INSTANCE_LAYOUT_MAT4X4, mat);
	
	bind_material_and_pipeline(cmd_buffer, visualization.material);
	draw_mesh(cmd_buffer, visualization.vertex_buffer, instance_buffer);
}
