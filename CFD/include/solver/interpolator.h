#pragma once

#include "field.h"
#include "matrix.h" //todo forward declare

//struct FV_ScalarMatrix;
//struct FV_VectorMatrix;
struct FV_Scalar_Data;
struct FV_Vector_Data;

struct FV_Patch;
struct FV_Wall_Patch;
struct FV_Interior_Patch;
struct FV_Inlet_Patch;

template<typename Field, typename Matrix, typename Data>
struct Interpolator {
    virtual void face_values(Field& result, const Data& data) const = 0;
    virtual void face_coeffs(Matrix& result, const Data& data) const = 0;
    virtual void face_grad(Field& result, const Data& data) const = 0;
    virtual void face_grad_coeffs(Matrix& result, const Data& data) const = 0;
    virtual void fix_boundary(Data& data) const {};
};

using ScalarInterpolator = Interpolator<ScalarField, FV_ScalarMatrix, FV_Scalar_Data>;
using VectorInterpolator = Interpolator<VectorField, FV_VectorMatrix, FV_Vector_Data>;

struct NoSlip : VectorInterpolator {
    FV_Wall_Patch& boundary;
    
    NoSlip(FV_Wall_Patch&);
    void face_values(VectorField& result, const FV_Vector_Data& data) const override;
    void face_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const override;
    void face_grad(VectorField& result, const FV_Vector_Data& data) const override;
    void face_grad_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const override;
    void fix_boundary(FV_Vector_Data& data) const;
};

struct FixedGradient : ScalarInterpolator {
    FV_Patch& patch;
    ScalarField grads;
    
    FixedGradient(FV_Patch&);
    void face_values(ScalarField& result, const FV_Scalar_Data& data) const override;
    void face_coeffs(FV_ScalarMatrix& result, const FV_Scalar_Data& data) const override;
    void face_grad(ScalarField& result, const FV_Scalar_Data& data) const override;
    void face_grad_coeffs(FV_ScalarMatrix& result, const FV_Scalar_Data& data) const override;
};

struct FaceAverage : ScalarInterpolator {
    FV_Interior_Patch& interior;
    
    FaceAverage(FV_Interior_Patch&);
    void face_values(ScalarField& result, const FV_Scalar_Data& data) const override;
    void face_coeffs(FV_ScalarMatrix& result, const FV_Scalar_Data& data) const override;
    void face_grad(ScalarField& result, const FV_Scalar_Data& data) const override;
    void face_grad_coeffs(FV_ScalarMatrix& result, const FV_Scalar_Data& data) const override;
};

struct FaceVecAverage : VectorInterpolator {
    FV_Interior_Patch& interior;

    FaceVecAverage(FV_Interior_Patch&);
    void face_values(VectorField& result, const FV_Vector_Data& data) const override;
    void face_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const override;
    void face_grad(VectorField& result, const FV_Vector_Data& data) const override;
    void face_grad_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const override;
};

struct ZeroVecGradient : VectorInterpolator {
    FV_Patch& boundary;

    ZeroVecGradient(FV_Patch&);
    void face_values(VectorField& result, const FV_Vector_Data& data) const override;
    void face_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const override;
    void face_grad(VectorField& result, const FV_Vector_Data& data) const override;
    void face_grad_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const override;
};

struct Upwind : VectorInterpolator {
    FV_Interior_Patch& interior;
    
    Upwind(FV_Interior_Patch&);
    void face_values(VectorField& result, const FV_Vector_Data& data) const override;
    void face_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const override;
    void face_grad(VectorField& result, const FV_Vector_Data& data) const override;
    void face_grad_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const override;
};


struct WaveGenerator : VectorInterpolator {
    FV_Inlet_Patch& patch;

    WaveGenerator(FV_Inlet_Patch&);
    void face_values(VectorField& result, const FV_Vector_Data& data) const override;
    void face_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const override;
    void face_grad(VectorField& result, const FV_Vector_Data& data) const override;
    void face_grad_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const override;
};
