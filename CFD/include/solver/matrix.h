#pragma once

#include "vendor/eigen/Eigen/Sparse"
#include "vendor/eigen/Eigen/IterativeLinearSolvers"
#include "cfd_core.h"
#include "field.h"
#include "core/container/vector.h"

struct FV_Mesh_Data;
struct FV_ScalarData;
struct FV_VectorData;

using SparseMatrix = Eigen::SparseMatrix<real>;

class FV_VectorMatrix {
    const FV_Mesh_Data& mesh;
    FV_VectorData& target;
    SparseMatrix sparse;
    VectorField source;
    
    inline auto seq(uint face, const VectorField& field) { return Eigen::seqN(face, field.outerSize()); }
    
public:
    VectorField face_cell_coeffs;
    VectorField face_neigh_coeffs;
    VectorField face_sources;
    
    FV_VectorMatrix(const FV_Mesh_Data& mesh, FV_VectorData& target);
    
    void add_coeffs(Sequence seq, const VectorField& coeff, const VectorField& neigh);
    void add_sources(Sequence seq, const VectorField& source);
    
    void operator*=(const VectorField& other);
    void operator-=(const FV_VectorMatrix& other);
    void operator+=(const FV_VectorMatrix& other);
    void build();
    void solve();
};

class FV_ScalarMatrix {
    const FV_Mesh_Data& mesh;
    FV_ScalarData& target;
    SparseMatrix sparse;
    ScalarField source;
    
    inline auto seq(uint face, const ScalarField& field) { return Eigen::seqN(face, field.outerSize()); }
    
public:
    ScalarField face_cell_coeffs;
    ScalarField face_neigh_coeffs;
    ScalarField face_sources;
    
    FV_ScalarMatrix(const FV_Mesh_Data& mesh, FV_ScalarData& target);
    
    void add_coeffs(Sequence seq, const ScalarField& coeff, const ScalarField& neigh);
    void add_sources(Sequence seq, const ScalarField& source);
    
    void operator*=(const ScalarField& other);
    void operator+=(const FV_ScalarMatrix& other);
    void operator-=(const FV_ScalarMatrix& other);
    void build();
    void solve();
};
