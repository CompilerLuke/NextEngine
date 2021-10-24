#pragma once

#include "field.h"

struct FV_ScalarMatrix;
struct FV_VectorMatrix;
struct FV_ScalarData;
struct FV_VectorData;

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
};

using ScalarInterpolator = Interpolator<ScalarField, FV_ScalarMatrix, FV_ScalarData>;
using VectorInterpolator = Interpolator<VectorField, FV_VectorMatrix, FV_VectorData>;

struct NoSlip : VectorInterpolator {
    FV_Wall_Patch& boundary;
    
    NoSlip(FV_Wall_Patch&);
    void face_values(VectorField& result, const FV_VectorData& data) const override;
    void face_coeffs(FV_VectorMatrix& result, const FV_VectorData& data) const override;
    void face_grad(VectorField& result, const FV_VectorData& data) const override;
    void face_grad_coeffs(FV_VectorMatrix& result, const FV_VectorData& data) const override;
};

struct FixedGradient : ScalarInterpolator {
    FV_Patch& patch;
    ScalarField grads;
    
    FixedGradient(FV_Patch&, ScalarField&& grad);
    void face_values(ScalarField& result, const FV_ScalarData& data) const override;
    void face_coeffs(FV_ScalarMatrix& result, const FV_ScalarData& data) const override;
    void face_grad(ScalarField& result, const FV_ScalarData& data) const override;
    void face_grad_coeffs(FV_ScalarMatrix& result, const FV_ScalarData& data) const override;
};

struct FaceAverage : ScalarInterpolator {
    FV_Interior_Patch& interior;
    
    FaceAverage(FV_Interior_Patch&);
    void face_values(ScalarField& result, const FV_ScalarData& data) const override;
    void face_coeffs(FV_ScalarMatrix& result, const FV_ScalarData& data) const override;
    void face_grad(ScalarField& result, const FV_ScalarData& data) const override;
    void face_grad_coeffs(FV_ScalarMatrix& result, const FV_ScalarData& data) const override;
};

struct Upwind : VectorInterpolator {
    FV_Interior_Patch& interior;
    
    Upwind(FV_Interior_Patch&);
    void face_values(VectorField& result, const FV_VectorData& data) const override;
    void face_coeffs(FV_VectorMatrix& result, const FV_VectorData& data) const override;
    void face_grad(VectorField& result, const FV_VectorData& data) const override;
    void face_grad_coeffs(FV_VectorMatrix& result, const FV_VectorData& data) const override;
};
