#pragma once

#include "patches.h"
#include "matrix.h"

namespace fvm {
    /*FV_ScalarMatrix divergence(FV_Scalar& scalar, const FV_Vector& vector) {
        FV_ScalarMatrix result = scalar.face_coeffs();
        auto result = vector.face_values();
        
        result &= scalar.mesh.sf();
        result *= scalar.face_values();
        return result;
    }*/

    FV_ScalarMatrix laplace(const ScalarField& a, FV_Scalar& scalar) {
        const ScalarField& fa = scalar.mesh.fa();
        FV_ScalarMatrix result = scalar.face_grad_coeffs();
        result *= a;
        result *= fa;
        return result;
    }

    FV_ScalarMatrix laplace(FV_Scalar& scalar) {
        const ScalarField& fa = scalar.mesh.fa();
        FV_ScalarMatrix result = scalar.face_grad_coeffs();
        result *= fa;
        return result;
    }

    FV_VectorMatrix laplace(FV_Vector& vector) {
        const ScalarField& fa = vector.mesh.fa();
        FV_VectorMatrix result = vector.face_grad_coeffs();
        result *= fa.replicate<3,1>();
        return result;
    }

    FV_VectorMatrix conv(const VectorField& a, FV_Vector& vec) {
        const FV_Mesh_Data& mesh = vec.mesh;
        FV_VectorMatrix faces = vec.face_coeffs();
        faces *= (mesh.fa() * vec_dot(a, mesh.normal)).replicate<3,1>();
        return faces;
    }

    FV_VectorMatrix ddt(FV_Vector& vec, VectorField& last, real dt) {
        const FV_Mesh_Data& mesh = vec.mesh;
        FV_VectorMatrix result(mesh, vec.data);

        auto seq = Eigen::seqN(0, mesh.cell_count);

        result.add_ccoeffs(seq, VectorField::Constant(3,mesh.cell_count,1.0/dt));
        result.add_csources(seq, last / dt);

        return result;
    }
}

namespace fvc {
    VectorField grad(const FV_Scalar& scalar) {
        const FV_Mesh_Data& mesh = scalar.mesh;
        const VectorField& sf = mesh.sf();
        VectorField faces = scalar.face_values().replicate<3,1>();
        
        faces.array() *= sf.array();

        const IndexField& cell_ids = mesh.cells();
        
        VectorField cells(VectorField::Zero(3, mesh.cell_count));
        cells(Eigen::all, mesh.cells()).array() += faces.array();
        cells.array() *= mesh.inv_volume.replicate<3,1>().array();

        auto min = cells.minCoeff();
        auto max = cells.maxCoeff();

        return cells;
    }

    VectorField conv(const VectorField& a, const FV_Vector& vec) {
        const FV_Mesh_Data& mesh = vec.mesh;
        VectorField faces = vec.face_values();
        faces *= (mesh.fa() * vec_dot(a, mesh.normal)).replicate<3, 1>();
        return faces;
    }

    ScalarField div(const FV_Vector& vec) {
        const FV_Mesh_Data& mesh = vec.mesh;
        VectorField faces = vec.face_values();
        VectorField cell_values = vec.values();

        /*std::cout << "Cell values" << std::endl;
        std::cout << cell_values << std::endl;

        std::cout << "Face values" << std::endl;
        std::cout << faces << std::endl;

        std::cout << "Normals" << std::endl;
        std::cout << mesh.normal << std::endl;

        std::cout << "Cell" << std::endl;
        std::cout << mesh.cell_ids << std::endl;
        std::cout << mesh.neigh_ids << std::endl;*/

        ScalarField dot = vec_dot(faces, mesh.sf());

        //std::cout << "Dot" << std::endl;
        //std::cout << dot << std::endl;

        ScalarField cells(ScalarField::Zero(1, mesh.cell_count));
        cells(Eigen::all, mesh.cells()) += dot;
        cells *= mesh.inv_volume;

        auto min = cells.minCoeff();
        auto max = cells.maxCoeff();

        return cells;
    }

    ScalarField div(const ScalarField& a, const FV_Vector& vec) {
        const FV_Mesh_Data& mesh = vec.mesh;
        VectorField faces = vec.face_values();
        faces *= a.replicate<3,1>();

        ScalarField dot = vec_dot(faces, mesh.normal);

        ScalarField cells(ScalarField::Zero(1, mesh.cell_count));
        cells(Eigen::all, mesh.cells()) += dot;
        cells *= mesh.inv_volume;

        auto min = cells.minCoeff();
        auto max = cells.maxCoeff();

        return cells;
    }

    TensorField grad(const FV_Vector& scalar) {
        return {};
    }
}