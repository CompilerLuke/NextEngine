#pragma once

#include "field.h"
#include "fmesh.h"
#include "interpolator.h"

struct FV_Patch {
    uint patch_begin;
    uint patch_count;

    const FV_Mesh_Data& mesh;

    FV_Patch(const FV_Mesh_Data& mesh, uint patch_begin, uint patch_count)
        : mesh(mesh), patch_begin(patch_begin), patch_count(patch_count) {}

    inline Sequence faces() { return Eigen::seqN(patch_begin, patch_count); }
    inline auto sf() { return mesh.normal(Eigen::all, faces()).cwiseProduct(mesh.area(faces()).replicate<3, 1>()); };
    inline auto normal() { return mesh.normal(Eigen::all, faces()); }
    inline auto ortho() { return mesh.ortho(Eigen::all, faces()); }
    inline auto area() { return mesh.area(Eigen::all, faces()); }
    inline auto fc() { return mesh.center_to_face(Eigen::all, faces()); }//(Eigen::all, seq()); }
    inline auto dx() { return mesh.inv_dx(Eigen::all, faces()); }//(Eigen::all, seq()); }
    inline auto cells() { return mesh.cell_ids(Eigen::all, faces()); }
    inline auto neighs() { return mesh.neigh_ids(Eigen::all, faces()); }
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

struct FV_Scalar_Data {
    ScalarField values;
    VectorField gradients;

    FV_Scalar_Data(uint count) : values(ScalarField::Zero(1, count)), gradients(VectorField::Zero(3, count)) {}
};

struct FV_Vector_Data {
    VectorField values;
    TensorField gradients;

    FV_Vector_Data(uint count) : values(VectorField::Zero(3, count)), gradients(1, count) {
        gradients.fill(Eigen::Matrix3d::Zero());
    }
};

template<typename Field, typename Matrix, typename Data>
class FV_Value {

    vector<Interpolator<Field, Matrix, Data>*> interp;

    static constexpr uint D = Field::RowsAtCompileTime;

public:    
    Data data;
    const FV_Mesh_Data& mesh;

    FV_Value(FV_Mesh_Data& mesh) : data(mesh.cell_count), mesh(mesh) {}

    void add(Interpolator<Field, Matrix, Data>* i) {
        interp.append(i);
    }

    void fix_boundary() {
        for (auto i : interp) { i->fix_boundary(data); }
    }

    Field face_values() const {
        Field result(Field::Zero(D, mesh.face_count));
        for (auto i : interp) { i->face_values(result, data); }
        return result;
    }

    Matrix face_coeffs() {
        Matrix result(mesh, data);
        for (auto i : interp) { i->face_coeffs(result, data); }
        return result;
    }

    Field face_grad() const {
        Field result(Field::Zero(D, mesh.face_count));
        for (auto i : interp) { i->face_grad(result, data); }
        return result;
    }

    Matrix face_grad_coeffs() {
        Matrix result(mesh, data);
        for (auto i : interp) { i->face_grad_coeffs(result, data); }
        return result;
    }

    Field& values() { return data.values; }
    auto& grads() { return data.gradients; }

    const Field& values() const { return data.values; }
    const auto& grads() const { return data.gradients; }
};

using FV_Scalar = FV_Value<ScalarField, FV_ScalarMatrix, FV_Scalar_Data>;
using FV_Vector = FV_Value<VectorField, FV_VectorMatrix, FV_Vector_Data>;



struct FV_Mesh {
    FV_Interior_Patch interior;
    FV_Wall_Patch wall;
    FV_Inlet_Patch inlet;
    std::unique_ptr<FV_Mesh_Data> data;
};

FV_Mesh build_mesh(struct CFDVolume& mesh, struct CFDDebugRenderer& debug);
