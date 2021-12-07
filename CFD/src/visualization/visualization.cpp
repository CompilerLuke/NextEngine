#include "visualization/visualizer.h"
#include "mesh.h"
#include "cfd_components.h"
#include "graphics/assets/model.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/draw.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "core/time.h"
#include "cfd_ids.h"

#include "core/math/vec3.h"
#include "core/math/vec4.h"
#include "visualization/render_backend.h"
#include "visualization/color_map.h"

#include "solver.h"

struct CFDVisualization {
    CFDRenderBackend& backend;
    CFDTriangleBuffer triangles[MAX_FRAMES_IN_FLIGHT];
    CFDLineBuffer lines[MAX_FRAMES_IN_FLIGHT];
    
    FlowQuantity last_quantity;
    vec4 last_plane;
    uint frame_index;
};

CFDVisualization* make_cfd_visualization(CFDRenderBackend& backend) {
    CFDVisualization* visualization = PERMANENT_ALLOC(CFDVisualization, {backend});
    alloc_triangle_buffer(backend, visualization->triangles, mb(200), mb(200));
    alloc_line_buffer(backend, visualization->lines, mb(200), mb(200));

	return visualization;
}

void build_vertex_representation(CFDVisualization& visualization, CFDVolume& mesh, vec4 plane, FlowQuantity quantity, CFDResults& results, bool rebuild) {
    
    if (!rebuild && plane == visualization.last_plane && quantity == visualization.last_quantity) return;
    
    //Identify contour
    uint* visible = TEMPORARY_ZEROED_ARRAY(uint, divceil(mesh.cells.length, 32));
    float epsilon = 0.1;
    
    for (int i = 0; i < mesh.cells.length; i++) {
        CFDCell& cell = mesh.cells[i];
        
        uint n = shapes[cell.type].num_verts;
        
#if 0
        bool is_visible = true;
        for (uint i = 0; i < n; i++) {
            vec3 position = mesh[cell.vertices[i]].position;
            
            if (dot(plane, position) < plane.w+epsilon) {
                is_visible = false;
                break;
            }
        }
#elif 1
        vec3 centroid = compute_centroid(mesh, cell.vertices, n);
        bool is_visible = dot(plane, centroid) > plane.w-epsilon;
        //is_visible = is_visible && dot(plane, centroid) < (plane.w+3.0)-epsilon;
#else
        bool is_visible = results.vof.length==0 || results.vof[i] > 0.8;
#endif
        
        //is_visible = true;
        if (is_visible) visible[i / 32] |= 1 << (i%32);
    }
    
	//line_vertices.resize(mesh.vertices.length);
	//for (uint i = 0; i < mesh.vertices.length; i++) {
	//	Vertex& vertex = line_vertices[i];
	//	vertex.position = mesh.vertices[i].position;
	//}
    
    visualization.last_plane = plane;
    visualization.last_quantity = quantity;

    visualization.frame_index = (visualization.frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
    
    CFDTriangleBuffer& triangle_buffer = visualization.triangles[visualization.frame_index];
    CFDLineBuffer& line_buffer = visualization.lines[visualization.frame_index];

    vec4 line_color = vec4(vec3(0,0,0), 1.0);
    vec4 face_color = vec4(vec3(1.0), 1.0);

    triangle_buffer.clear();
    line_buffer.clear();

    uint line_offset = line_buffer.vertex_arena.offset;
    for (CFDVertex vertex : mesh.vertices) {
        line_buffer.append({ vec4(vertex.position,0.0), line_color });
    }
    
    bool has_results = results.velocities.length > 0;
    
	for (uint i = 0; i < mesh.cells.length; i++) {
        if (i % 32 == 0 && visible[i/32] == 0) {
            i += 31;
            continue;
        }
        
        if (!(visible[i/32] & (1 << i%32))) continue;
        
        const CFDCell& cell = mesh.cells[i];
        const ShapeDesc& desc = shapes[cell.type];
        
#if 1
        bool is_contour = false;
        for (uint i = 0; i < desc.num_faces; i++) {
            int neigh = cell.faces[i].neighbor.id;
            if (neigh > 10000000) {
                printf("Invalid neighbor!!\n");
            }
            bool has_no_neighbor = neigh == -1 || !(visible[neigh/32] & (1 << neigh%32));
            
            if (has_no_neighbor) {
                is_contour = true;
                break;
            }
        }
        
        if (!is_contour) continue;
#endif

        float size = 0;
        uint count = 0.0f;
        for (uint i = 0; i < desc.num_faces; i++) {
            const ShapeDesc::Face& face = desc.faces[i];

            for (uint j = 0; j < desc[i].num_verts; j++) {
                vertex_handle v0 = cell.vertices[face.verts[j]];
                vertex_handle v1 = cell.vertices[face.verts[(j + 1) % face.num_verts]];

                size += length(mesh[v0].position - mesh[v1].position);
                count++;
            }
        }
        
        size /= count;
        vec4 cell_color;
        vec3 velocity;
        if (has_results) {
            velocity = results.velocities[i] / results.max_velocity;
            //cell_color = color_map(length(velocity), 0, results.max_velocity);
            if (quantity == FlowQuantity::Pressure) {
                cell_color = color_map(results.pressures[i], results.min_pressure, results.max_pressure);
            }
            if (quantity == FlowQuantity::Velocity) cell_color = color_map(length(results.velocities[i]), 0, results.max_velocity);
            if (quantity == FlowQuantity::PressureGradient) {
                cell_color = color_map(length(results.pressure_grad[i]), 0, results.max_pressure_grad);
                velocity = results.pressure_grad[i] / results.max_pressure_grad;
            }
        } else {
            cell_color = color_map(log2f(size), -5, 5);
        }

        for (uint i = 0; i < desc.num_faces; i++) {
            const ShapeDesc::Face& face = desc.faces[i];
            
            uint triangle_offset = triangle_buffer.vertex_arena.offset;
            vec3 face_normal = cell.faces[i].normal;

            vec4 face_color = cell_color;
            if (cell.faces[i].pressure != 0.0f) {
                face_color = RED_DEBUG_COLOR;
            }
            if (cell.faces[i].velocity != 0.0f) {
                face_color = BLUE_DEBUG_COLOR;
            }

            vec3 centroid;
            for (uint j = 0; j < face.num_verts; j++) {
                vertex_handle v0 = cell.vertices[face.verts[j]];
                vertex_handle v1 = cell.vertices[face.verts[(j+1) % face.num_verts]];

                line_buffer.append(line_offset + v0.id);
                line_buffer.append(line_offset + v1.id);
                
                triangle_buffer.append({vec4(mesh[v0].position,0), vec4(face_normal,1.0), face_color});
                
                centroid += mesh[v0].position;
            }
            centroid /= face.num_verts;
            
            if (has_results) {
                const real arrow = 0.2; // *length(velocity);
                
                vec3 dir = normalize(velocity) * size * 0.8 * length(velocity);
                vec3 bitangent = normalize(cross(dir, plane)) * size * arrow * length(velocity);
                
                centroid += face_normal * size * 0.01;
                vec3 start = centroid - 0.5*dir;
                vec3 end = centroid + 0.5*dir;
                
                line_buffer.draw_line(start, end, vec4(0,0,0,1));
                line_buffer.draw_line(end, end + bitangent - dir*arrow, vec4(0,0,0,1));
                line_buffer.draw_line(end, end - bitangent - dir*arrow, vec4(0,0,0,1));
            }

            triangle_buffer.append(triangle_offset);
            triangle_buffer.append(triangle_offset + 1);
            triangle_buffer.append(triangle_offset + 2);

            if (face.num_verts == 4) {
                triangle_buffer.append(triangle_offset);
                triangle_buffer.append(triangle_offset + 2);
                triangle_buffer.append(triangle_offset + 3);
            }
        }
	}    
}

void render_cfd_mesh(CFDVisualization& visualization, CommandBuffer& cmd_buffer) {
    CFDRenderBackend& backend = visualization.backend;

    auto& triangles = visualization.triangles[visualization.frame_index];
    auto& lines = visualization.lines[visualization.frame_index];
    
    render_triangle_buffer(backend, cmd_buffer, triangles, triangles.index_arena.offset);
    render_line_buffer(backend, cmd_buffer, lines, lines.index_arena.offset);
}
