#pragma once

#include "fmesh.h"
#include "matrix.h"

namespace fvm {
    /*FV_ScalarMatrix divergence(FV_Scalar& scalar, const FV_Vector& vector) {
        FV_ScalarMatrix result = scalar.face_coeffs();
        auto result = vector.face_values();
        
        result &= scalar.mesh.sf();
        result *= scalar.face_values();
        return result;
    }*/

    FV_ScalarMatrix laplace(FV_Scalar& scalar) {
        const ScalarField& sf = scalar.mesh.fa();
        FV_ScalarMatrix result = scalar.face_grad_coeffs();
        result *= sf;
        return result;
    }

    FV_VectorMatrix laplace(FV_Vector& vector) {
        const ScalarField& fa = vector.mesh.fa();
        FV_VectorMatrix result = vector.face_grad_coeffs();
        result *= fa.replicate<3,1>();
        return result;
    }
}
