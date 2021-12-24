#pragma once

#include "vendor/eigen/Eigen/Sparse"
#include "vendor/eigen/Eigen/IterativeLinearSolvers"
#include "cfd_core.h"
#include "field.h"
#include "fmesh.h"
#include "core/container/vector.h"
#include <iostream>

struct FV_Mesh_Data;
struct FV_Scalar_Data;
struct FV_Vector_Data;

using SparseMatrix = Eigen::SparseMatrix<real, Eigen::RowMajor>;

template<typename Data, uint D>
class FV_Matrix {
public:
    using Field = Eigen::Array<real, D, Eigen::Dynamic>;
    using Matrix = FV_Matrix<Data, D>;
    using Triplet = Eigen::Triplet<real>;
    using Value = Eigen::Array<real, D, 1>;

    bool should_rebuild = true;
    Data& target;
    SparseMatrix sparse[D];
    Field cell_sources;

    int ref_cell = -1;
    Value ref_value;
    real under_relax_coeff = 1.0;

    void dirty(bool rebuild = true) {
        should_rebuild = rebuild;
    }

    bool rebuild() {
        return should_rebuild;
    }

    const FV_Mesh_Data& mesh;

    Field source;
    Field cell_coeffs;
    Field face_cell_coeffs;
    Field face_neigh_coeffs;
    Field face_sources;

    FV_Matrix(const FV_Mesh_Data& mesh, Data& target) : mesh(mesh),
        target(target),
        face_cell_coeffs(Field::Zero(D, mesh.face_count)),
        face_neigh_coeffs(Field::Zero(D, mesh.face_count)),
        face_sources(Field::Zero(D, mesh.face_count)),
        
        cell_coeffs(Field::Zero(D, mesh.cell_count)),
        cell_sources(Field::Zero(D, mesh.cell_count)),
    
        source(Field::Zero(D, mesh.cell_count)) {

    }

    void set_ref(uint cell, real value) {
        dirty();
        ref_cell = cell;
        ref_value = Eigen::Vector<real,1>(value);
    }

    void set_ref(uint cell, vec3 value) {
        dirty();
        ref_cell = cell;
        ref_value = Eigen::Vector<real,3>(value.x, value.y, value.z);
    }

    void add_fcoeffs(Sequence seq, const Field& neigh, const Field& coeff) {
        dirty();
        face_cell_coeffs(Eigen::all, seq) += coeff;
        face_neigh_coeffs(Eigen::all, seq) += neigh;

        assert(neigh.allFinite());
        assert(coeff.allFinite());
    }

    void add_ccoeffs(Sequence seq, const Field& coeff) {
        dirty();
        cell_coeffs(Eigen::all, seq) += coeff;
    }

    void add_csources(Sequence seq, const Field& source) {
        dirty();
        cell_sources(Eigen::all, seq) += source;
    }

    void add_fsources(Sequence seq, const Field& source) {
        dirty();
        face_sources(Eigen::all, seq) += source;

        assert(source.allFinite());
    }

    void operator*=(const Field& other) {
        bool over_cells = other.cols() == mesh.cell_count;
        bool over_faces = other.cols() == mesh.face_count;

        assert(over_cells || over_faces);
        assert(other.allFinite());

        dirty();

        if (over_faces) {
            face_cell_coeffs.array() *= other.array();
            face_neigh_coeffs.array() *= other.array();
            face_sources.array() *= other.array();
        }

        if (over_cells) {
            auto other_faces = other(Eigen::all, mesh.cells());

            face_cell_coeffs.array() *= other_faces.array();
            face_neigh_coeffs.array() *= other_faces.array();
            face_sources.array() *= other_faces.array();

            cell_coeffs.array() *= other.array();
            cell_sources.array() *= other.array();
        }
    }

    void operator+=(const Field& other) {
        dirty();
        cell_sources += other;
    }

    void operator-=(const Field& other) {
        dirty();
        cell_sources -= other;
    }

    void operator+=(const Matrix& other) {
        dirty();
        face_cell_coeffs += other.face_cell_coeffs;
        face_neigh_coeffs += other.face_neigh_coeffs;
        face_sources += other.face_sources;
        
        cell_coeffs += other.cell_coeffs;
        cell_sources += other.cell_sources;
    }

    void operator-=(const Matrix& other) {
        dirty();
        face_cell_coeffs -= other.face_cell_coeffs;
        face_neigh_coeffs -= other.face_neigh_coeffs;
        face_sources -= other.face_sources;  

        cell_coeffs -= other.cell_coeffs;
        cell_sources -= other.cell_sources;
    }

    Field A() {
        Field result(D, mesh.cell_count);
        for (uint i = 0; i < D; i++) {
            result(i,Eigen::all) = sparse[i].diagonal();
        }
        return result;
    }

    Field H() {
        Field result(D, mesh.cell_count);
        for (uint i = 0; i < D; i++) {
            auto values = target.values(i,Eigen::all).matrix().transpose();

            //MU = AU - H
            //H = AU - MU

            result(i, Eigen::all) =
                sparse[i]
                .diagonal()
                .cwiseProduct(values);
            -sparse[i] * values;
                //+ source(i, Eigen::all).matrix().transpose();
        }
        
        return result;
    }

    void build() {
        if (!rebuild()) return;
        dirty(false);

        vector<Triplet> coeffs[D];

        for (uint i = 0; i < D; i++) {
            coeffs[i].resize(mesh.face_count*2 + mesh.cell_count);
            sparse[i].resize(mesh.cell_count, mesh.cell_count);
        }
        
        source = Field::Zero(D, mesh.cell_count);

        //todo factor into function
        for (uint i = 0; i < mesh.face_count; i++) {
            uint cell = mesh.cell_ids(i);
            uint neigh = mesh.neigh_ids(i);
            real inv_volume = mesh.inv_volume(cell);
            
            auto cell_coeff = face_cell_coeffs(Eigen::all, i) * inv_volume;
            auto neigh_coeff = face_neigh_coeffs(Eigen::all, i) * inv_volume;
            auto face_source = face_sources(Eigen::all, i) * inv_volume;
            
            for (uint axis = 0; axis < D; axis++) {
                coeffs[axis].append(Triplet(cell, cell, cell_coeff(axis)));
                coeffs[axis].append(Triplet(cell, neigh, neigh_coeff(axis)));
            }
            
            source(Eigen::all, cell) += -face_source;
        }

        source += cell_sources;

        for (uint cell = 0; cell < mesh.cell_count; cell++) {
            real inv_volume = mesh.inv_volume(cell);
            auto cell_coeff = cell_coeffs(Eigen::all, cell);

            for (uint axis = 0; axis < D; axis++) {
                coeffs[axis].append(Triplet(cell, cell, cell_coeff(axis)));
            }
        }

        for (uint axis = 0; axis < D; axis++) {
            sparse[axis].setFromTriplets(coeffs[axis].begin(), coeffs[axis].end());
        }

        if (under_relax_coeff != 1.0) {
            real factor = (1 - under_relax_coeff) / under_relax_coeff;

            for (uint i = 0; i < D; i++) {
                auto sum =
                source.row(i) += sparse[i].diagonal().cwiseProduct(target.values.row(i).transpose().matrix()).array() * factor;
                //(factor * mesh.inv_volume * target.values.row(i));
                //
                sparse[i].diagonal() *= 1 + factor;
                (factor * mesh.inv_volume).transpose().matrix();
                //sparse[i].diagonal() * factor;
            }
        }
        
        if (ref_cell != -1) {
            /*for (SparseMatrix::InnerIterator it(sparse, ref_cell); it; ++it) {
                it.valueRef() = 0.0;
            }*/
            for(uint i = 0; i<D; i++) {
                for (SparseMatrix::InnerIterator it(sparse[i], ref_cell); it; ++it) {
                    it.valueRef() = 0.0;
                }
                sparse[i].diagonal()(ref_cell) = 1.0;
            }
            source.col(ref_cell) = ref_value;
        }
    }

    void under_relax(real alpha) {
        this->under_relax_coeff = alpha;
    }

    float solve() {
        Eigen::BiCGSTAB<SparseMatrix> solver;

        build();

        Eigen::Array<real, 1, Eigen::Dynamic> new_value;

        real change = 0.0f;
        
        //std::cout << "=============" << std::endl;

        for (uint axis = 0; axis < D; axis++) {
            //std::cout << "=======" << std::endl;
            //std::cout << sparse[axis] << std::endl;
            //std::cout << source.row(axis) << std::endl;

            solver.compute(sparse[axis]);
            new_value = solver.solveWithGuess(source.row(axis).transpose().matrix(), target.values.row(axis).transpose().matrix()).transpose();
            
            auto old_value = target.values.row(axis);
            
            change += ((new_value - old_value)/old_value).cwiseAbs().mean();
            target.values.row(axis) = new_value;
        
            //std::cout << "Solution" << std::endl;
            //std::cout << target.values.row(axis) << std::endl;
        }

        change /= D;

        //std::cout << target.values << std::endl;
        return change;
    }
};

using FV_ScalarMatrix = FV_Matrix<FV_Scalar_Data, 1>;
using FV_VectorMatrix = FV_Matrix<FV_Vector_Data, 3>;

#define MATRIX_OP(Type, Dim, Op) inline FV_##Type##Matrix operator Op (real value, FV_##Type##Matrix&& matrix) { \
    uint face_count = matrix.mesh.face_count; \
    FV_##Type##Matrix result = std::move(matrix); \
    result Op##= Type##Field::Constant(Dim, face_count, value); \
    return result; \
} \
inline FV_##Type##Matrix operator Op (const Type##Field& value, FV_##Type##Matrix&& matrix) { \
    FV_##Type##Matrix result = std::move(matrix); \
    result Op##= value; \
    return result; \
}

MATRIX_OP(Scalar, 1, *)
MATRIX_OP(Scalar, 1, +)
MATRIX_OP(Scalar, 1, -)

MATRIX_OP(Vector, 3, *)
MATRIX_OP(Vector, 3, +)
MATRIX_OP(Vector, 3, -)

inline FV_ScalarMatrix operator==(FV_ScalarMatrix&& matrix, const ScalarField& source) {
    FV_ScalarMatrix result = std::move(matrix);
    result += source;
    return result;
}

inline FV_VectorMatrix operator==(FV_VectorMatrix&& matrix, const VectorField& source) {
    FV_VectorMatrix result = std::move(matrix);
    result += source;
    return result;
}

inline FV_VectorMatrix operator+(FV_VectorMatrix&& matrix, const FV_VectorMatrix& other) {
    FV_VectorMatrix result = std::move(matrix);
    result += other;
    return result;
}

inline FV_VectorMatrix operator-(FV_VectorMatrix&& matrix, const FV_VectorMatrix& other) {
    FV_VectorMatrix result = std::move(matrix);
    result -= other;
    return result;
}

inline FV_VectorMatrix operator+(FV_VectorMatrix&& matrix, Eigen::Vector3d constant) {
    FV_VectorMatrix result = std::move(matrix);
    result += constant.replicate(1, matrix.mesh.face_count).array();
    return result;
}

inline FV_VectorMatrix operator-(FV_VectorMatrix&& matrix, Eigen::Vector3d constant) {
    FV_VectorMatrix result = std::move(matrix);
    result += -constant.replicate(1, matrix.mesh.face_count).array();
    return result;
}

inline FV_VectorMatrix operator-(FV_VectorMatrix&& matrix) {
    FV_VectorMatrix result = std::move(matrix);
    result *= VectorField::Constant(3, matrix.mesh.face_count, -1); //todo add - operator matrix
    return result;
}

/*
inline FV_ScalarMatrix vec_dot(const FV_VectorMatrix& a, const VecField& b) {
    FV_ScalarMatrix result(a.mesh);
    result.face_cell_coeffs = vec_dot(face_cell_coeffs, other);
    result.face_neigh_coeffs = vec_dot(face_neigh_coeffs, other);
    result.face_sources = vec_dot(face_sources, other);

    return result
}
*/
