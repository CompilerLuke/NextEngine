#include "solver/matrix.h"
#include "core/container/vector.h"
#include "solver/fmesh.h"

/*
FV_ScalarMatrix::FV_ScalarMatrix(const FV_Mesh_Data& mesh, FV_Scalar_Data& target) 
    : mesh(mesh), 
      target(target),
      face_cell_coeffs(ScalarField::Zero(1,mesh.face_count)), 
      face_neigh_coeffs(ScalarField::Zero(1,mesh.face_count)),
      face_sources(ScalarField::Zero(1,mesh.face_count)) {
    
}

void FV_ScalarMatrix::add_coeffs(Sequence seq, const ScalarField& neigh, const ScalarField& coeff) {
    rebuild = true;
    face_cell_coeffs(Eigen::all, seq) += coeff;
    face_neigh_coeffs(Eigen::all, seq) += neigh;
}

void FV_ScalarMatrix::add_sources(Sequence seq, const ScalarField &source) {
    rebuild = true;
    face_sources(Eigen::all, seq) = source;

    assert(source.maxCoeff() < 1e5);
    assert(!source.hasNaN());
}

void FV_ScalarMatrix::operator*=(const ScalarField &other) {
    rebuild = true;
    face_cell_coeffs.array() *= other.array();
    face_neigh_coeffs.array() *= other.array();
    face_sources.array() *= other.array();
}

void FV_ScalarMatrix::operator+=(const FV_ScalarMatrix &other) {
    rebuild = true;
    face_cell_coeffs += other.face_cell_coeffs;
    face_neigh_coeffs += other.face_neigh_coeffs;
    face_sources += other.face_sources;
}

void FV_ScalarMatrix::operator-=(const FV_ScalarMatrix &other) {
    rebuild = true;
    face_cell_coeffs -= other.face_cell_coeffs;
    face_neigh_coeffs -= other.face_neigh_coeffs;
    face_sources -= other.face_sources;
}

using Triplet = Eigen::Triplet<real>;

void FV_ScalarMatrix::build() {
    if (!rebuild) return;
    rebuild = false;

    vector<Triplet> coeffs;
        
    sparse.resize(mesh.cell_count, mesh.cell_count);
    source.resize(1,mesh.cell_count);
    source.setZero();
    
    for (uint i = 0; i < mesh.face_count; i++) {
        uint cell = mesh.cell_ids(i);
        uint neigh = mesh.neigh_ids(i);
        real cell_coeff = face_cell_coeffs(i);
        real neigh_coeff = face_neigh_coeffs(i);
        
        coeffs.append(Triplet(cell, cell, cell_coeff));
        coeffs.append(Triplet(cell, neigh, neigh_coeff));
        source(cell) += face_sources(i);
    }
    

    sparse.setFromTriplets(coeffs.begin(), coeffs.end());
}

void FV_ScalarMatrix::set_ref(uint cell, real value) {
    build();

    for (SparseMatrix::InnerIterator it(sparse, cell); it; ++it) {
        it.valueRef() = 0.0;
    }
    //sparse.row(cell) = Eigen::Cons 0.0f;
    sparse.diagonal()(cell) += 1.0;
    source(cell) += value;
}

#include <iostream>

void FV_ScalarMatrix::solve() {
    Eigen::BiCGSTAB<SparseMatrix> solver;
    
    build();

    //std::cout << "=======" << std::endl;
    //std::cout << sparse << std::endl;
    //std::cout << source << std::endl;
    
    solver.compute(sparse);
    target.values = solver.solveWithGuess(source.transpose(), target.values.transpose()).transpose();

    //std::cout << target.values << std::endl;
}

FV_VectorMatrix::FV_VectorMatrix(const FV_Mesh_Data& mesh, FV_Vector_Data& target)
    : mesh(mesh), target(target), face_cell_coeffs(3, mesh.face_count), face_neigh_coeffs(3, mesh.face_count), face_sources(3, mesh.face_count) {
    
}

void FV_VectorMatrix::add_coeffs(Sequence seq, const VectorField& neigh, const VectorField& coeff) {
    face_cell_coeffs(Eigen::all, seq) += coeff;
    face_neigh_coeffs(Eigen::all, seq) += neigh;
}

void FV_VectorMatrix::add_sources(Sequence seq, const VectorField &source) {
    assert(source.size()/3 == source.outerSize());
    face_sources(Eigen::all, seq) = source;
}

void FV_VectorMatrix::operator*=(const VectorField &other) {
    face_cell_coeffs = face_cell_coeffs.cwiseProduct(other);
    face_neigh_coeffs = face_neigh_coeffs.cwiseProduct(other);
    face_sources = face_sources.cwiseProduct(other);
}

void FV_VectorMatrix::operator+=(const FV_VectorMatrix &other) {
    face_cell_coeffs += other.face_cell_coeffs;
    face_neigh_coeffs += other.face_neigh_coeffs;
    face_sources += other.face_sources;
}

void FV_VectorMatrix::operator-=(const FV_VectorMatrix &other) {
    face_cell_coeffs -= other.face_cell_coeffs;
    face_neigh_coeffs -= other.face_neigh_coeffs;
    face_sources -= other.face_sources;
}

VectorField tensor_dot(const TensorField& tensor, const VectorField& vec) {
   uint n = vec.cols();
   VectorField result(3,n);
   
   for (uint i = 0; i < n; i++) {
       result.col(i) = tensor(i) * vec.col(i);
   }
   return result;
}
*/
