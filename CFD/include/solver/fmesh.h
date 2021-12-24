#pragma once

#include "core/container/vector.h"
#include "field.h"

struct FV_Mesh_Data {
    uint cell_count;
    uint face_count;
    
    VectorField center_pos;
    ScalarField inv_volume;
    VectorField ortho;
    VectorField normal;
    ScalarField area;
    VectorField center_to_face;
    ScalarField inv_dx;
    IndexField cell_ids;
    IndexField neigh_ids;
    
    auto& centers() const { return center_pos; }
    auto& cells() const { return cell_ids; }
    auto& neighs() const { return neigh_ids; }
    auto& cf() const { return ortho; }
    auto sf() const { return normal.array() * area.replicate<3, 1>().array(); };
    auto& fa() const { return area; }
    auto& fc() const { return center_to_face; }
    auto& dx() const { return inv_dx; }
};
