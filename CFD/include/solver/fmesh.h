#pragma once

#include "core/container/vector.h"
#include "field.h"
#include "interpolator.h"

struct FV_Mesh_Data {
    uint cell_count;
    uint face_count;
    
    ScalarField inv_volume;
    VectorField ortho;
    VectorField normal;
    ScalarField area;
    VectorField center_to_face;
    ScalarField inv_dx;
    IndexField cell_ids;
    IndexField neigh_ids;
    
    auto& cells() const { return cell_ids; }
    auto& neighs() const { return neigh_ids; }
    auto& cf() const { return ortho; }
    auto& sf() const { return normal; }; //* area.replicate<3,1>();
    auto& fa() const { return area; }
    auto& fc() const { return center_to_face; }
    auto& dx() const { return inv_dx; }
};

struct FV_Patch {
    uint patch_begin;
    uint patch_count;
    
    const FV_Mesh_Data& mesh;
    
    FV_Patch(const FV_Mesh_Data& mesh, uint patch_begin, uint patch_count)
    : mesh(mesh), patch_begin(patch_begin), patch_count(patch_count) {}
    
    inline Sequence faces() { return Eigen::seqN(patch_begin, patch_count); }
    inline const VectorField& sf() { return mesh.normal; }; //* mesh.area.replicate<3,1>();
    inline const VectorField& normal() { return mesh.normal; }
    inline const ScalarField& area() { return mesh.area; }
    inline const VectorField& fc() { return mesh.center_to_face; }//(Eigen::all, seq()); }
    inline const ScalarField& dx() { return mesh.inv_dx; }//(Eigen::all, seq()); }
    inline const IndexField& cells() { return mesh.cell_ids; }
    inline const IndexField& neighs() { return mesh.neigh_ids; }
};

struct FV_Interior_Patch : public FV_Patch {
    VectorField neigh_center_to_face;
    ScalarField dx_cell_weight;
    ScalarField dx_neigh_weight;
    
    FV_Interior_Patch(const FV_Mesh_Data& mesh, uint patch_begin, uint patch_count)
    : FV_Patch(mesh, patch_begin, patch_count) {}
    
    auto& cell_weight() const { return dx_cell_weight; }
    auto& neigh_weight() const { return dx_neigh_weight; }
};

struct FV_Wall_Patch : public FV_Patch {
    FV_Wall_Patch(const FV_Mesh_Data& mesh, uint patch_begin, uint patch_count)
    : FV_Patch(mesh, patch_begin, patch_count) {}
};

struct FV_Inlet_Patch : public FV_Patch {
    FV_Inlet_Patch(const FV_Mesh_Data& mesh, uint patch_begin, uint patch_count) : FV_Patch(mesh, patch_begin, patch_count) {}
};

/*
template<typename... Patches>
class FV_Mesh {
    const FV_Mesh_Data& mesh;
    
    std::tuple<Patches...> patches;
    
public:
    FV_Mesh(const Patches&... p)
    : patches(p...),
      mesh(std::get<0>(patches).mesh)
    {
        
    }
    
    uint cell_count() const { return mesh.cell_count; }
    uint face_count() { return mesh.face_count; }
    
    auto& sf() const { return mesh.normal; };
    auto& area() const { return mesh.area; }
    auto& fc() const { return mesh.face_to_center; }
    auto& dx() const { return mesh.face_to_center_dx; }
};
 */

struct FV_ScalarData {
    ScalarField values;
    VectorField gradients;
    
    FV_ScalarData(uint count) : values(), gradients() {}
};

struct FV_VectorData {
    VectorField values;
    TensorField gradients;
    
    FV_VectorData(uint count) : values(), gradients() {}
};

template<typename Field, typename Matrix, typename Data>
class FV_Value {
    Data data;
    vector<Interpolator<Field, Matrix, Data>*> interp;

public:
    const FV_Mesh_Data& mesh;
    
    FV_Value(FV_Mesh_Data& mesh) : data(mesh.cell_count), mesh(mesh) {}
    
    void add(Interpolator<Field, Matrix, Data>* i) {
        interp.append(i);
    }
    
    Field face_values() const {
        Field result(mesh, data);
        for (auto i : interp) { i->face_values(result, data); }
        return result;
    }
    
    Matrix face_coeffs() {
        Matrix result(mesh, data);
        for (auto i : interp) { i->face_coeffs(result, data); }
        return result;
    }
    
    Field face_grad() const {
        Field result(mesh, data);
        for (auto i : interp) { i->face_grad(result, data); }
        return result;
    }
    
    Matrix face_grad_coeffs() {
        Matrix result(mesh, data);
        for (auto i : interp) { i->face_grad_coeffs(result, data); }
        return result;
    }
};

using FV_Scalar = FV_Value<ScalarField, FV_ScalarMatrix, FV_ScalarData>;
using FV_Vector = FV_Value<VectorField, FV_VectorMatrix, FV_VectorData>;



struct FV_Mesh {
    FV_Interior_Patch interior;
    FV_Wall_Patch wall;
    FV_Inlet_Patch inlet;
    std::unique_ptr<FV_Mesh_Data> data;
};

FV_Mesh build_mesh(struct CFDVolume& mesh);
