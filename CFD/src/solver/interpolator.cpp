#pragma once

#include "solver/interpolator.h"
#include "solver/fmesh.h"
#include "solver/matrix.h"

//Boundary Conditions
NoSlip::NoSlip(FV_Wall_Patch& wall) : boundary(wall) {}
void NoSlip::face_values(VectorField& result, const FV_VectorData& data) const {}
void NoSlip::face_coeffs(FV_VectorMatrix& result, const FV_VectorData& data) const {}
void NoSlip::face_grad(VectorField& result, const FV_VectorData& data) const {
    auto cells = boundary.cells();
    
    auto delta = -data.values(Eigen::all, cells);
    result(Eigen::all, boundary.faces()) = delta.cwiseProduct(boundary.dx().replicate<3,1>());
}

void NoSlip::face_grad_coeffs(FV_VectorMatrix& result, const FV_VectorData& data) const {
    face_grad(result.face_sources, data);
}

FixedGradient::FixedGradient(FV_Patch& patch, ScalarField&& grads) : patch(patch), grads(std::move(grads)) {}

auto fixed_gradient_face_values(FV_Patch& patch, const ScalarField& grads, const FV_ScalarData& data) {
    auto cells = patch.cells();
    auto values = data.values(Eigen::all, cells);
    auto faces = values + grads.cwiseProduct(patch.dx().cwiseInverse());
    return faces;
}

void FixedGradient::face_values(ScalarField &result, const FV_ScalarData &data) const {
    result(Eigen::all, patch.faces()) += fixed_gradient_face_values(patch, grads, data);
}

void FixedGradient::face_coeffs(FV_ScalarMatrix &result, const FV_ScalarData &data) const {
    result.add_sources(patch.faces(), fixed_gradient_face_values(patch, grads, data));
}

void FixedGradient::face_grad(ScalarField &result, const FV_ScalarData &data) const {
    result(Eigen::all, patch.faces()) += grads;
}

void FixedGradient::face_grad_coeffs(FV_ScalarMatrix &result, const FV_ScalarData &data) const {
    result.add_sources(patch.faces(), grads);
}

//Interpolators
FaceAverage::FaceAverage(FV_Interior_Patch& interior) : interior(interior) {}
void FaceAverage::face_values(ScalarField& result, const FV_ScalarData& data) const {
    auto cell_values = data.values(Eigen::all, interior.cells());
    auto neigh_values = data.values(Eigen::all, interior.neighs());
   
    auto face = cell_values.cwiseProduct(interior.cell_weight()) + neigh_values.cwiseProduct(interior.neigh_weight());
            
    result(Eigen::all, interior.faces()) += face;
}

void FaceAverage::face_coeffs(FV_ScalarMatrix &result, const FV_ScalarData &data) const {
    result.add_coeffs(interior.faces(), interior.cell_weight(), interior.neigh_weight());
}

auto grad_ortho_corrector(FV_Interior_Patch& patch, const FV_ScalarData& data) {
    auto cell_grad_norm = data.gradients(Eigen::all, patch.cells()).transpose() * patch.normal();
    auto neigh_grad_norm = data.gradients(Eigen::all, patch.neighs()).transpose() * patch.normal();
    
    return cell_grad_norm.cwiseProduct(patch.cell_weight()) + neigh_grad_norm.cwiseProduct(patch.neigh_weight());
}

void FaceAverage::face_grad(ScalarField &result, const FV_ScalarData &data) const {
    auto cell_values = data.values(interior.cells());
    auto neigh_values = data.values(interior.neighs());
    
    auto corrector = grad_ortho_corrector(interior, data);
    result(Eigen::all, interior.faces()) = (neigh_values - cell_values).cwiseProduct(interior.dx()) + corrector;
}

void FaceAverage::face_grad_coeffs(FV_ScalarMatrix& result, const FV_ScalarData& data) const {
    auto cells = interior.cells();
    auto neighs = interior.neighs();
    auto cell_grad = data.gradients(Eigen::all, cells);
    auto neigh_grad = data.gradients(Eigen::all, neighs);
    
    auto corrector = grad_ortho_corrector(interior, data);

    result.add_sources(interior.faces(), corrector);
    result.add_coeffs(interior.faces(), -interior.dx(), interior.dx());
}

Upwind::Upwind(FV_Interior_Patch& interior) : interior(interior) {}
void Upwind::face_values(VectorField& result, const FV_VectorData& data) const {
    auto cells = interior.cells();
    auto neighs = interior.neighs();
    
    auto downwind = data.values(Eigen::all, cells);
    auto upwind = data.values(Eigen::all, neighs);
    
    auto normal = interior.sf();
    
    auto downwind_flux = normal.transpose() * upwind;
    auto upwind_flux = normal.transpose() * downwind;
    
    auto flux = (downwind_flux.array() > upwind_flux.array()).select(downwind, upwind);
    result(Eigen::all, cells) += flux;
}

void Upwind::face_coeffs(FV_VectorMatrix& result, const FV_VectorData& data) const {
    auto cells = interior.cells();
    auto neighs = interior.neighs();
    
    auto downwind = data.values(Eigen::all, cells);
    auto upwind = data.values(Eigen::all, neighs);
    
    auto normal = interior.sf();
    
    auto downwind_flux = normal.transpose() * upwind;
    auto upwind_flux = normal.transpose() * downwind;
    
    auto mask = downwind_flux.array() > upwind_flux.array();
    
    auto dx = VectorField::Constant(0,0,1.0);
    
    auto cell_coeff = mask.select(dx, 0);
    auto neigh_coeff = mask.select(0, dx);

    result.add_coeffs(interior.faces(), cell_coeff, neigh_coeff);
}

void Upwind::face_grad(VectorField &result, const FV_VectorData &data) const {
    auto cells = interior.cells();
    auto neighs = interior.neighs();
    auto dx = interior.dx().replicate<3,1>();
    auto cell_values = data.values(cells);
    auto neigh_values = data.values(neighs);
    auto cell_grad = data.gradients(cells);
    auto neigh_grad = data.gradients(neighs);
    
    auto cell_grad_norm = tensor_dot(cell_grad, interior.normal());
    auto neigh_grad_norm = tensor_dot(neigh_grad, interior.normal());
    
    VectorField grad = (cell_grad_norm + neigh_grad_norm) / 2.0f;
    
    result(Eigen::all, interior.faces()) += grad + dx.cwiseProduct(neigh_values - cell_values);
}
    
void Upwind::face_grad_coeffs(FV_VectorMatrix& result, const FV_VectorData& data) const {
    auto cells = interior.cells();
    auto neighs = interior.neighs();
    auto dx = interior.dx().replicate<3,1>();
    auto cell_grad = data.gradients(cells);
    auto neigh_grad = data.gradients(neighs);
    
    auto cell_grad_norm = tensor_dot(cell_grad, interior.normal());
    auto neigh_grad_norm = tensor_dot(neigh_grad, interior.normal());
    
    auto grad = (cell_grad_norm.array() + neigh_grad_norm.array()) / 2.0f;
    
    result.add_sources(interior.faces(), grad);
    result.add_coeffs(interior.faces(), dx, -dx);
}
