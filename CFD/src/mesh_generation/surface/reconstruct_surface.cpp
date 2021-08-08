#include "thekla/thekla_atlas.h"
#include "mesh/surface_tet_mesh.h"
#include "core/memory/linear_allocator.h"
#include "core/math/vec2.h"
#include "visualization/debug_renderer.h"

using namespace Thekla;

void reconstruct_surface(SurfaceTriMesh& surface, CFDDebugRenderer& debug, slice<vec3> points, slice<tri_handle> tris) {
    Atlas_Input_Vertex* vertex_array = TEMPORARY_ZEROED_ARRAY(Atlas_Input_Vertex, surface.tri_count*surface.N);
    Atlas_Input_Face* face_array = TEMPORARY_ZEROED_ARRAY(Atlas_Input_Face, surface.tri_count);
    
    for (tri_handle tri : surface) {
        vec3 v[3];
        surface.triangle_verts(tri, v);

        vec3 normal = triangle_normal(v);

        uint offset = tri - 3;

        Atlas_Input_Face& face = face_array[offset/3];
        face.material_index = 0;

        for (uint i = 0; i < 3; i++) {
            Atlas_Input_Vertex& vertex = vertex_array[offset + i];
            vertex.position[0] = v[i].x;
            vertex.position[1] = v[i].y;
            vertex.position[2] = v[i].z;

            vertex.normal[0] = normal.x;
            vertex.normal[1] = normal.y;
            vertex.normal[2] = normal.z;

            vertex.uv[0] = 0;
            vertex.uv[1] = 0;

            vertex.first_colocal = surface.indices[tri + i].id;

            face.vertex_index[i] = offset + i;
        }
    }

    Atlas_Input_Mesh mesh;
    mesh.face_count = surface.tri_count - 1;
    mesh.face_array = face_array;
    mesh.vertex_count = (surface.tri_count - 1) * 3;
    mesh.vertex_array = vertex_array;

    Atlas_Options options;
    atlas_set_default_options(&options);

    Atlas_Error error = {};
    Atlas_Output_Mesh* output = atlas_generate(&mesh, &options, &error);

    for (uint i = 0; i < output->index_count; i += 3) {
        vec3 v[3];
        float height = 5;

        for (uint j = 0; j < 3; j++) {
            uint index = output->index_array[i + j];
            vec2 uv = {
                output->vertex_array[index].uv[0],
                output->vertex_array[index].uv[1],
            };

            uv *= 0.1;
            //uv *= 5.0;

            v[j] = vec3(uv.x, height, uv.y);
        }

        vec3 normal = triangle_normal(v);
        float pointing_up = dot(normal, vec3(0, 1, 0)) > 0;
        if (!pointing_up) std::swap(v[0], v[2]);

        draw_triangle(debug, v, RED_DEBUG_COLOR);
    }

    suspend_execution(debug);
    atlas_free(output);
}