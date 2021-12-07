#include "solver/patches.h"
#include "mesh.h"
#include "core/math/vec4.h"
#include "core/math/intersection.h"
#include <memory>
#include "visualization/debug_renderer.h"


void right_angle_corrector(vec3 normal, vec3 to_center, real* parallel, vec3* ortho) {
    to_center = normalize(to_center);
    *parallel = dot(normal, to_center);
    *ortho = normal - to_center*(*parallel);
}

void right_angle_corrector(vec3 normal, vec3 to_center, real* parallel, vec3* ortho);

FV_Mesh build_mesh(CFDVolume& mesh, CFDDebugRenderer& debug) {
    std::unique_ptr<FV_Mesh_Data> data = std::make_unique<FV_Mesh_Data>();
    
    uint interior_count = 0;
    uint wall_count = 0;
    uint inlet_count = 0;

    clear_debug_stack(debug);
    
    for (uint i = 0; i < mesh.cells.length; i++) {
        const CFDCell& cell = mesh.cells[i];
        const ShapeDesc& shape = shapes[cell.type];
        
        for (uint f = 0; f < shape.num_faces; f++) {
            const CFDCell::Face& face = cell.faces[f];
            
            bool boundary = face.neighbor.id == -1;
            if (boundary && face.pressure != 0) inlet_count++;
            else if (boundary) wall_count++;
            else interior_count++;
        }
    }
    
    uint offset = 0;
    uint interior_base = offset;
    offset += interior_count;
    
    uint wall_base = offset;
    offset += wall_count;
    
    uint inlet_base = offset;
    offset += inlet_count;
    
    uint interior_offset = interior_base;
    uint wall_offset = wall_base;
    uint inlet_offset = inlet_base;
    
    FV_Mesh result = {
        FV_Interior_Patch(*data, interior_base, interior_count),
        FV_Wall_Patch(*data, wall_base, wall_count),
        FV_Inlet_Patch(*data, inlet_base, inlet_count),
    };

    uint face_count = offset;
    uint cell_count = mesh.cells.length;

    data->cell_count = cell_count;
    data->face_count = face_count;

    data->inv_volume.resize(Eigen::NoChange, cell_count);

    data->ortho.resize(Eigen::NoChange, offset);
    data->normal.resize(Eigen::NoChange, offset);
    data->cell_ids.resize(Eigen::NoChange, offset);
    data->center_to_face.resize(Eigen::NoChange, offset);
    data->neigh_ids.resize(Eigen::NoChange, offset);
    data->inv_dx.resize(Eigen::NoChange, offset);
    data->area.resize(Eigen::NoChange, offset);
    
    result.interior.dx_cell_weight.resize(interior_count);
    result.interior.dx_neigh_weight.resize(interior_count);
    result.interior.neigh_center_to_face.resize(Eigen::NoChange, interior_count);

    for (uint cell_id = 0; cell_id < mesh.cells.length; cell_id++) {
        const CFDCell& cell = mesh.cells[cell_id];
        const ShapeDesc& shape = shapes[cell.type];
        
        vec3 center = compute_centroid(mesh, cell.vertices, shape.num_verts);
        real volume = 0.0f;
        
        for (uint f = 0; f < shape.num_faces; f++) {
            const CFDCell::Face& face = cell.faces[f];
            const ShapeDesc::Face& face_shape = shape[f];
            
            bool boundary = face.neighbor.id == -1;
            bool inlet = boundary && face.pressure;
            bool wall = boundary && !inlet;
            
            vec3 face_center;
            vec3 v[4];
            for (uint j = 0; j < face_shape.num_verts; j++) {
                v[j] = mesh.vertices[cell.vertices[face_shape[j]].id].position;
                face_center += v[j];
            }
            face_center /= face_shape.num_verts;

            bool is_quad = face_shape.num_verts == 4;

            real area = is_quad ? quad_area(v) : triangle_area(v);
            vec3 normal = is_quad ? quad_normal(v) : triangle_normal(v);
            
            volume += dot(face_center, area * normal);

            vec3 center_to_face;
            vec3 cell_to_neigh;
            
            vec4 face_plane = vec4(normal, -dot(normal, face_center));
            
            uint face_id;
            uint neigh = 0;
            if (boundary) {
                vec3 inter = face_center;
                //ray_plane_intersect(Ray(center, normal), face_plane, &inter);
                //todo determine intersection

                if (inlet) {
                    face_id = inlet_offset++;
                }
                if (wall) {
                    face_id = wall_offset++;
                }

                cell_to_neigh = inter - center;
                data->ortho.col(face_id).setZero();
            }
            else {
                face_id = interior_offset++;
                
                const CFDCell& neigh_cell = mesh[face.neighbor];
                neigh = face.neighbor.id;
                
                vec3 neigh_center = compute_centroid(mesh, neigh_cell.vertices, shapes[neigh_cell.type].num_verts);
                
                draw_line(debug, center, neigh_center, dot(neigh_center-center,normal) > 0 ? RED_DEBUG_COLOR : GREEN_DEBUG_COLOR);

                cell_to_neigh = neigh_center - center;
                
                vec3 inter = 0.5*center + 0.5*neigh_center;
                
                //todo determine intersection
                //ray_plane_intersect(Ray(center, cell_to_neigh), face_plane, &inter);
                
                center_to_face = inter - center;
                vec3 neigh_to_face = inter - neigh_center;
                
                assert(length(cell_to_neigh) > 1e-5);
                real w0 = length(neigh_to_face) / length(cell_to_neigh);
                real w1 = 1.0 - w0;
                
                uint patch_id = face_id - interior_base;
                
                vec3 ortho;
                real parallel;
                
                right_angle_corrector(normal, center_to_face, &parallel, &ortho);
                
                result.interior.neigh_center_to_face.col(patch_id) = Eigen::Vector3d(neigh_to_face.x, neigh_to_face.y, neigh_to_face.z);
                result.interior.dx_cell_weight(0, patch_id) = w0;
                result.interior.dx_neigh_weight(0, patch_id) = w1;
            }

            data->ortho.col(face_id).setZero(); //todo orthogonal correctors
            data->area(0,face_id) = area;
            data->inv_dx(0,face_id) = 1.0/length(cell_to_neigh);
            data->center_to_face.col(face_id) = Eigen::Vector3d{center_to_face.x, center_to_face.y, center_to_face.z};
            data->normal.col(face_id) = Eigen::Vector3d{normal.x, normal.y, normal.z};
            data->cell_ids(0,face_id) = cell_id;
            data->neigh_ids(0,face_id) = neigh;

            assert(std::isfinite(data->inv_dx(0,face_id)));
        }

        data->inv_volume(cell_id) = 3.0/volume;

        suspend_execution(debug);
    }

    //suspend_execution(debug);
    
    result.data = std::move(data);

    assert(result.interior.dx_cell_weight.allFinite());
    assert(result.interior.dx_neigh_weight.allFinite());
    assert(result.data->inv_dx.allFinite());
    assert(result.data->ortho.allFinite());
    assert(result.data->normal.allFinite());
    assert(result.data->area.allFinite());

    /*
    
        uint cell_id = i;

        vec3 center = compute_centroid(mesh, cell.vertices, shape.num_verts);

        sim.centers[i] = center;

        real volume = 0.0f;
        
        bool is_on_boundary = is_boundary(cell);

        for (uint f = 0; f < shape.num_faces; f++) {
            const ShapeDesc::Face& face = shape[f];

            
            cell_handle neighbor = cell.faces[f].neighbor;
            bool boundary = neighbor.id == -1;

            volume += dot(face_center, normal);

            if (boundary) {
                vec3 cell_to_face = face_center - center;
                real dx = length(cell_to_face);
                real inv_dx = 1.0 / (2.0*dx) * area;
                if (cell.faces[f].pressure != 0.0f) {
                    draw_quad(debug, v, BLUE_DEBUG_COLOR);
                    sim.pressure_boundary_faces.append(PressureBoundaryFace{ inv_dx, cell.faces[f].pressure*dx, normal, cell.faces[f].pressure, cell_id });
                }
                else {
                    sim.boundary_faces.append(BoundaryFace{ inv_dx, normal, cell_to_face, cell_id, cell.faces[f].velocity });
                }
            }
            else if (neighbor.id > cell_id) {
                sim.faces.append(Face{0, normal, 0, cell_id, (uint)neighbor.id, is_on_boundary, is_boundary(mesh[neighbor])});
            }
        }

        volume /= 3;
        sim.inv_volumes[cell_id] = 1.0 / volume;
    }

    for (Face& face : sim.faces) {
        vec3 center_minus = sim.centers[face.cell1];
        vec3 center_plus = sim.centers[face.cell2];
        
        vec3 t = center_plus - center_minus;

        real parallel;
        right_angle_corrector(face.normal, center_plus - center_minus, &parallel, &face.ortho);
        face.dx = 1.0 / length(t) * parallel;
    }
     */


    
    return result;
}
