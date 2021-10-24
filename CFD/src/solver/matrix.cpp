#include "solver/matrix.h"
#include "core/container/vector.h"
#include "solver/fmesh.h"

FV_ScalarMatrix::FV_ScalarMatrix(const FV_Mesh_Data& mesh, FV_ScalarData& target) : mesh(mesh), target(target) {
    
}

void FV_ScalarMatrix::add_coeffs(Sequence seq, const ScalarField& coeff, const ScalarField& neigh) {
    face_cell_coeffs(Eigen::all, seq) += coeff;
    face_neigh_coeffs(Eigen::all, seq) += neigh;
}

void FV_ScalarMatrix::add_sources(Sequence seq, const ScalarField &source) {
    face_sources(Eigen::all, seq) = source;
}

void FV_ScalarMatrix::operator*=(const ScalarField &other) {
    face_cell_coeffs = face_cell_coeffs.cwiseProduct(other);
    face_neigh_coeffs = face_neigh_coeffs.cwiseProduct(other);
}

void FV_ScalarMatrix::operator+=(const FV_ScalarMatrix &other) {
    face_cell_coeffs += other.face_cell_coeffs;
    face_neigh_coeffs += other.face_neigh_coeffs;
    face_sources += other.face_sources;
}

void FV_ScalarMatrix::operator-=(const FV_ScalarMatrix &other) {
    face_cell_coeffs -= other.face_cell_coeffs;
    face_neigh_coeffs -= other.face_neigh_coeffs;
    face_sources -= other.face_sources;
}

using Triplet = Eigen::Triplet<real>;

void FV_ScalarMatrix::build() {
    vector<Triplet> coeffs;
    
    for (uint i = 0; i < mesh.face_count; i++) {
        uint cell = mesh.cell_ids(i);
        uint neigh = mesh.neigh_ids(i);
        real cell_coeff = face_cell_coeffs(i);
        real neigh_coeff = face_cell_coeffs(i);
        
        coeffs.append(Triplet(cell, cell, cell_coeff));
        coeffs.append(Triplet(cell, neigh, neigh_coeff));
    }
    
    sparse.setFromTriplets(coeffs.begin(), coeffs.end());
}

void FV_ScalarMatrix::solve() {
    Eigen::BiCGSTAB<SparseMatrix> solver;
    
    build();
    
    solver.compute(sparse);
    target.values = solver.solveWithGuess(source, target.values);
}

FV_VectorMatrix::FV_VectorMatrix(const FV_Mesh_Data& mesh, FV_VectorData& target) : mesh(mesh), target(target) {
    
}

void FV_VectorMatrix::add_coeffs(Sequence seq, const VectorField &coeff, const VectorField &neigh) {
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

