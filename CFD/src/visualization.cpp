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

#include "core/math/vec3.h"
#include "core/math/vec4.h"

struct DebugRenderer {
	CPUVisibleBuffer debug_line_buffer;
	CPUVisibleBuffer debug_index_buffer;
	Arena vertex_arena[MAX_FRAMES_IN_FLIGHT];
	Arena index_arena[MAX_FRAMES_IN_FLIGHT];
	uint frame_index;

	pipeline_handle pipeline_line;
	pipeline_handle pipeline_triangle;
};

/*struct CFD_GPU_Vertex {
    vec3 position;
    vec3 normal;
};*/

struct CFDVisualization {
    uint frame_index;
    vec4 last_plane;
    Arena vertex_arena[MAX_FRAMES_IN_FLIGHT];
    Arena index_arena[MAX_FRAMES_IN_FLIGHT];
    CPUVisibleBuffer vertex_buffer;
    CPUVisibleBuffer index_buffer;
    VertexLayout vertex_layout;
	VertexBuffer line_vertex_buffer;
	VertexBuffer solid_vertex_buffer;
	pipeline_handle pipeline_triangle_solid;
	pipeline_handle pipeline_triangle_wireframe;
	pipeline_handle pipeline_line;
};

CFDVisualization* make_cfd_visualization() {
	CFDVisualization* visualization = PERMANENT_ALLOC(CFDVisualization);
	
    /*VertexLayoutDesc desc = { {
            {3, VertexAttrib::Float, offsetof(CFD_GPU_Vertex, position)},
            {3, VertexAttrib::Float, offsetof(CFD_GPU_Vertex, normal)}
        }, sizeof(CFD_GPU_Vertex),
    };

    visualization->vertex_layout = register_vertex_layout(desc);
     */
    
    visualization->vertex_layout = VERTEX_LAYOUT_DEFAULT;
    
    alloc_Arena(visualization->vertex_arena, mb(200), &visualization->vertex_buffer, BUFFER_VERTEX);
    alloc_Arena(visualization->index_arena, mb(200), &visualization->index_buffer, BUFFER_INDEX);
        
	shader_flags flags = 0;
	shader_handle shader = load_Shader("shaders/CFD/color.vert", "shaders/CFD/color.frag", flags);

	GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.vertex_layout = visualization->vertex_layout;
	pipeline_desc.instance_layout = INSTANCE_LAYOUT_NONE;
	pipeline_desc.shader = shader;
	pipeline_desc.shader_flags = flags;
	pipeline_desc.subpass = 1;
	pipeline_desc.render_pass = RenderPass::Scene;
	pipeline_desc.range[PushConstant_Vertex].size = sizeof(glm::mat4);
	pipeline_desc.range[PushConstant_Fragment].size = sizeof(glm::vec4);
	pipeline_desc.range[PushConstant_Fragment].offset = sizeof(glm::mat4);
    //pipeline_desc.state = ; //temporary
    
	visualization->pipeline_triangle_solid = query_Pipeline(pipeline_desc);

	pipeline_desc.state = Cull_None | PolyMode_Wireframe | (3 << WireframeLineWidth_Offset);
	visualization->pipeline_triangle_wireframe = query_Pipeline(pipeline_desc);

	pipeline_desc.state = Cull_None | PrimitiveType_LineList | (3 << WireframeLineWidth_Offset);
	visualization->pipeline_line = query_Pipeline(pipeline_desc);

	return visualization;
}

void build_vertex_representation(CFDVisualization& visualization, CFDVolume& mesh, vec4 plane) {
	//Identify contour
    uint* visible = TEMPORARY_ZEROED_ARRAY(uint, (mesh.cells.length+1) / 32);
    
    for (int i = 0; i < mesh.cells.length; i++) {
        CFDCell& cell = mesh.cells[i];
        
        uint n = shapes[cell.type].num_verts;
        
#if 1
        bool is_visible = true;
        for (uint i = 0; i < n; i++) {
            vec3 position = mesh[cell.vertices[i]].position;
            
            if (dot(plane, position) < plane.w-FLT_EPSILON) {
                is_visible = false;
                break;
            }
        }
#else
        vec3 centroid = compute_centroid(mesh, cell.vertices, n);
        bool is_visible = dot(plane, centroid) > plane.w;
#endif
        
        if (is_visible) visible[i / 32] |= 1 << i%32;
    }
    
	//line_vertices.resize(mesh.vertices.length);
	//for (uint i = 0; i < mesh.vertices.length; i++) {
	//	Vertex& vertex = line_vertices[i];
	//	vertex.position = mesh.vertices[i].position;
	//}
    
    visualization.frame_index = (visualization.frame_index+1) % MAX_FRAMES_IN_FLIGHT;
    
    Arena& vertex_arena = visualization.vertex_arena[visualization.frame_index];
    Arena& index_arena = visualization.index_arena[visualization.frame_index];
    
    uint vertex_count = 0;
    uint solid_index_count = 0;
    uint line_index_count = 0;

    u64 line_buffer_offset = index_arena.capacity / 2;
    Vertex* solid_vertices = (Vertex*)((u8*)visualization.vertex_buffer.mapped + vertex_arena.base_offset);
    uint* solid_indices = (uint*)((u8*)visualization.index_buffer.mapped + index_arena.base_offset);
    uint* line_indices = (uint*)((u8*)solid_indices + line_buffer_offset);
    
	for (uint i = 0; i < mesh.cells.length; i++) {
        if (i % 32 == 0 && visible[i/32] == 0) {
            i += 31;
            continue;
        }
        
        if (!(visible[i/32] & (1 << i%32))) continue;
        
        const CFDCell& cell = mesh.cells[i];
        const ShapeDesc& desc = shapes[cell.type];
        
        /*bool is_contour = false;
        for (uint i = 0; i < desc.num_faces; i++) {
            int neigh = cell.faces[i].neighbor.id;
            bool has_no_neighbor = neigh == -1 || !(visible[neigh/32] & (1 << neigh%32));
            
            if (has_no_neighbor) {
                is_contour = true;
                break;
            }
        }
        
        if (!is_contour) continue;*/
        
        for (uint i = 0; i < desc.num_faces; i++) {
            const ShapeDesc::Face& face = desc.faces[i];
            
            uint offset = vertex_count;
            vec3 face_normal = cell.faces[i].normal;

            for (uint j = 0; j < face.num_verts; j++) {
                vertex_handle v = cell.vertices[face.verts[j]];

                line_indices[line_index_count++] = offset + j;
                line_indices[line_index_count++] = offset + (j+1) % face.num_verts;
                
                solid_vertices[vertex_count++] = {mesh[v].position, face_normal};
            }

            solid_indices[solid_index_count++] = offset;
            solid_indices[solid_index_count++] = offset + 1;
            solid_indices[solid_index_count++] = offset + 2;

            if (face.num_verts == 4) {
                solid_indices[solid_index_count++] = offset;
                solid_indices[solid_index_count++] = offset + 2;
                solid_indices[solid_index_count++] = offset + 3;
            }
        }
	}
    
    visualization.line_vertex_buffer = {};
    visualization.solid_vertex_buffer = {};
    
    visualization.line_vertex_buffer.length = line_index_count;
    visualization.solid_vertex_buffer.length = solid_index_count;
    
    visualization.line_vertex_buffer.index_base = line_buffer_offset / sizeof(uint);

	//todo leaks memory
	//begin_gpu_upload();
	//visualization.line_vertex_buffer = alloc_vertex_buffer<Vertex>(VERTEX_LAYOUT_DEFAULT, line_vertices, line_indices);
	//visualization.solid_vertex_buffer = alloc_vertex_buffer<Vertex>(VERTEX_LAYOUT_DEFAULT, solid_vertices, solid_indices);
	//end_gpu_upload();
}


void render_cfd_mesh(CFDVisualization& vis, CommandBuffer& cmd_buffer) {
	glm::mat4 mat(1.0);
	glm::vec4 color(1.0, 0.0, 0.0, 1.0);
	glm::vec4 line_color(0.0, 0.0, 0.0, 1.0);
    
    uint frame_index = vis.frame_index;
	bind_pipeline(cmd_buffer, vis.pipeline_triangle_solid);
    bind_vertex_buffer(cmd_buffer, vis.vertex_buffer.buffer, vis.vertex_arena[frame_index].base_offset);
    bind_index_buffer(cmd_buffer, vis.index_buffer.buffer, vis.index_arena[frame_index].base_offset);
	
    push_constant(cmd_buffer, VERTEX_STAGE, 0, &mat);
	
	push_constant(cmd_buffer, FRAGMENT_STAGE, sizeof(glm::mat4), &color);
	draw_mesh(cmd_buffer, vis.solid_vertex_buffer);
		
	bind_pipeline(cmd_buffer, vis.pipeline_line);
	push_constant(cmd_buffer, FRAGMENT_STAGE, sizeof(glm::mat4), &line_color);
	draw_mesh(cmd_buffer, vis.line_vertex_buffer);
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
	//return;
	
	bind_pipeline(cmd_buffer, visualization.pipeline_triangle_solid);
	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_NONE);

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
