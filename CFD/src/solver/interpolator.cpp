#if 1

#include "solver/interpolator.h"
#include "solver/patches.h"
#include "solver/matrix.h"

using Eigen::all;

//Boundary Conditions
NoSlip::NoSlip(FV_Patch& wall) : boundary(wall), values(VectorField::Zero(3, wall.patch_count)) {}
void NoSlip::face_values(VectorField& result, const FV_Vector_Data& data) const {
    result(Eigen::all, boundary.faces()) += values;
}
void NoSlip::face_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const {
    result.add_fsources(boundary.faces(), values);
}
void NoSlip::face_grad(VectorField& result, const FV_Vector_Data& data) const {
    auto cells = boundary.cells();
    
    auto delta = values - data.values(Eigen::all, cells);
    result(Eigen::all, boundary.faces()) += delta.cwiseProduct(boundary.dx().replicate<3,1>());
}

void NoSlip::face_grad_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const {
    uint count = boundary.patch_count;
    result.add_fcoeffs(boundary.faces(), values, -boundary.dx().replicate<3, 1>());
}

void NoSlip::fix_boundary(FV_Vector_Data& data) const {
    ScalarField normal_component = vec_dot(data.values(all,boundary.cells()), boundary.normal());
    data.values(Eigen::all,boundary.cells()) -= boundary.normal() * normal_component.replicate<3,1>();
}

FixedGradient::FixedGradient(FV_Patch& patch) : patch(patch), grads(ScalarField::Zero(1, patch.patch_count)) {}

auto fixed_gradient_face_values(FV_Patch& patch, const ScalarField& grads, const FV_Scalar_Data& data) {
    auto cells = patch.cells();
    auto values = data.values(Eigen::all, cells);
    ScalarField dx = patch.dx().cwiseInverse();
    ScalarField faces = values + grads.cwiseProduct(dx);
    return faces;
}

void FixedGradient::face_values(ScalarField &result, const FV_Scalar_Data &data) const {
    result(Eigen::all, patch.faces()) += fixed_gradient_face_values(patch, grads, data);
}

void FixedGradient::face_coeffs(FV_ScalarMatrix &result, const FV_Scalar_Data &data) const {
    uint count = patch.patch_count;
    result.add_fcoeffs(patch.faces(), ScalarField::Constant(1,count,0), ScalarField::Constant(1,count,1));
    result.add_fsources(patch.faces(), grads.cwiseProduct(patch.dx().cwiseInverse()));
}

void FixedGradient::face_grad(ScalarField &result, const FV_Scalar_Data &data) const {
    result(Eigen::all, patch.faces()) += grads;
}

void FixedGradient::face_grad_coeffs(FV_ScalarMatrix &result, const FV_Scalar_Data &data) const {
    result.add_fsources(patch.faces(), grads);
}

//Wave Generator
/*auto fixed_gradient_face_values(FV_Patch& patch, const FV_Scalar_Data& data) {
    auto cells = patch.cells();
    auto values = data.values(Eigen::all, cells);
    auto faces = values + grads.cwiseProduct(patch.dx().cwiseInverse());
    return faces;
}*/

WaveGenerator::WaveGenerator(FV_Inlet_Patch& patch) : patch(patch) {}

void WaveGenerator::face_values(VectorField& result, const FV_Vector_Data& data) const {
    result(Eigen::all, patch.faces()) += data.values(Eigen::all, patch.cells());
}

void WaveGenerator::face_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const {
    uint count = patch.patch_count;
    result.add_fcoeffs(patch.faces(), VectorField::Constant(3,count, 0), VectorField::Constant(3,count, 1));
}

void WaveGenerator::face_grad(VectorField& result, const FV_Vector_Data& data) const {
    uint count = patch.patch_count;
    result(Eigen::all, patch.faces()) += VectorField::Constant(3,count, 0);
}

void WaveGenerator::face_grad_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const {
    uint count = patch.patch_count;
    result.add_fsources(patch.faces(), VectorField::Constant(3,count, 0));
}

//Interpolators
FaceAverage::FaceAverage(FV_Interior_Patch& interior) : interior(interior) {}
void FaceAverage::face_values(ScalarField& result, const FV_Scalar_Data& data) const {
    auto cell_values = data.values(Eigen::all, interior.cells());
    auto neigh_values = data.values(Eigen::all, interior.neighs());
   
    auto face = cell_values.cwiseProduct(interior.cell_weight()) + neigh_values.cwiseProduct(interior.neigh_weight());
            
    result(Eigen::all, interior.faces()) += face;
}

void FaceAverage::face_coeffs(FV_ScalarMatrix &result, const FV_Scalar_Data &data) const {
    result.add_fcoeffs(interior.faces(), interior.cell_weight(), interior.neigh_weight());
}

auto grad_ortho_corrector(FV_Interior_Patch& patch, const FV_Scalar_Data& data) {
    auto grad_cells = data.gradients(Eigen::all, patch.cells());
    auto grad_neighs = data.gradients(Eigen::all, patch.neighs());
    VectorField normal = patch.ortho();

    ScalarField cell_grad_norm = vec_dot(grad_cells, normal);
    ScalarField neigh_grad_norm = vec_dot(grad_neighs, normal);

    assert(!normal.hasNaN());
    assert(!cell_grad_norm.hasNaN());
    assert(!neigh_grad_norm.hasNaN());
    assert(!patch.cell_weight().hasNaN());
    assert(!patch.neigh_weight().hasNaN());

    ScalarField result = cell_grad_norm.cwiseProduct(patch.cell_weight()) + neigh_grad_norm.cwiseProduct(patch.neigh_weight());
    return ScalarField::Zero(1, patch.patch_count); //result;
}

void FaceAverage::face_grad(ScalarField &result, const FV_Scalar_Data &data) const {
    auto cell_values = data.values(interior.cells());
    auto neigh_values = data.values(interior.neighs());
    
    auto corrector = grad_ortho_corrector(interior, data);
    result(Eigen::all, interior.faces()) = (neigh_values - cell_values).cwiseProduct(interior.dx()) + corrector;
}

void FaceAverage::face_grad_coeffs(FV_ScalarMatrix& result, const FV_Scalar_Data& data) const {
    auto cells = interior.cells();
    auto neighs = interior.neighs();
    auto cell_grad = data.gradients(Eigen::all, cells);
    auto neigh_grad = data.gradients(Eigen::all, neighs);
    
    auto corrector = grad_ortho_corrector(interior, data);

    result.add_fsources(interior.faces(), corrector);
    result.add_fcoeffs(interior.faces(), interior.dx(), -interior.dx());
}

FaceVecAverage::FaceVecAverage(FV_Interior_Patch& interior) : interior(interior) {

}

void FaceVecAverage::face_values(VectorField& result, const FV_Vector_Data& data) const {
    auto cell_weight = interior.cell_weight().replicate<3,1>();
    auto neigh_weight = interior.neigh_weight().replicate<3, 1>();

    auto cell_values = data.values(all, interior.cells()).cwiseProduct(cell_weight);
    auto neigh_values = data.values(all, interior.neighs()).cwiseProduct(neigh_weight);
    
    result(all, interior.faces()) += cell_values + neigh_values;
}
void FaceVecAverage::face_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const {}
void FaceVecAverage::face_grad(VectorField& result, const FV_Vector_Data& data) const {}
void FaceVecAverage::face_grad_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const {}

ZeroVecGradient::ZeroVecGradient(FV_Patch& patch) : boundary(patch), grads(VectorField::Zero(3,patch.patch_count)) {

}



void ZeroVecGradient::face_values(VectorField& result, const FV_Vector_Data& data) const {
    result(all,boundary.faces()) += data.values(all,boundary.cells()) + grads * boundary.dx().cwiseInverse().replicate<3,1>();
}
void ZeroVecGradient::face_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const {
    auto coeff = VectorField::Ones(3, boundary.faces().size());
    result.add_fcoeffs(boundary.faces(), coeff, coeff);
    result.add_fsources(boundary.faces(), grads * boundary.dx().cwiseInverse().replicate<3,1>());
}
void ZeroVecGradient::face_grad(VectorField& result, const FV_Vector_Data& data) const {
    result(all,boundary.faces()) = grads;
}
void ZeroVecGradient::face_grad_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const {
    result.add_fsources(boundary.faces(), grads);
}



Upwind::Upwind(FV_Interior_Patch& interior) : interior(interior) {}
void Upwind::face_values(VectorField& result, const FV_Vector_Data& data) const {
    auto cells = interior.cells();
    auto neighs = interior.neighs();
    
    VectorField downwind = data.values(Eigen::all, cells);
    VectorField upwind = data.values(Eigen::all, neighs);
    
    auto normal = interior.normal();
    
    auto flux = vec_dot(normal, upwind + downwind);
    
    VectorField value = (flux >= 0).replicate<3,1>().select(downwind, upwind);
    result(Eigen::all, interior.faces()) += 0.5*(downwind + upwind);
}

void Upwind::face_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const {
    auto cells = interior.cells();
    auto neighs = interior.neighs();
    
    auto downwind = data.values(Eigen::all, cells);
    auto upwind = data.values(Eigen::all, neighs);
    
    auto normal = interior.normal();
    
    auto flux = vec_dot(normal, upwind+downwind);
    //auto upwind_flux = vec_dot(normal, downwind);
    
    auto mask = flux.array() >= 0; // upwind_flux.array();
    
    auto dx = VectorField::Constant(3,interior.patch_count,1.0);
    
    auto cell_coeff = mask.replicate<3,1>().select(dx, 0);
    auto neigh_coeff = mask.replicate<3,1>().select(0, dx);

    result.add_fcoeffs(interior.faces(), 0.5*dx, 0.5*dx);
}

void Upwind::face_grad(VectorField &result, const FV_Vector_Data &data) const {
    auto cells = interior.cells();
    auto neighs = interior.neighs();
    auto dx = interior.dx().replicate<3,1>();
    auto cell_values = data.values(Eigen::all, cells);
    auto neigh_values = data.values(Eigen::all, neighs);
    auto cell_grad = data.gradients(cells);
    auto neigh_grad = data.gradients(neighs);
    
    auto cell_grad_norm = tensor_dot(cell_grad, interior.normal());
    auto neigh_grad_norm = tensor_dot(neigh_grad, interior.normal());
    
    VectorField grad = (cell_grad_norm + neigh_grad_norm) / 2.0f;
    
    result(Eigen::all, interior.faces()) += grad + dx.cwiseProduct(neigh_values - cell_values);
}
    
void Upwind::face_grad_coeffs(FV_VectorMatrix& result, const FV_Vector_Data& data) const {
    auto cells = interior.cells();
    auto neighs = interior.neighs();
    auto dx = interior.dx().replicate<3,1>();
    auto cell_grad = data.gradients(cells);
    auto neigh_grad = data.gradients(neighs);
    
    auto cell_grad_norm = tensor_dot(cell_grad, interior.ortho());
    auto neigh_grad_norm = tensor_dot(neigh_grad, interior.ortho());
    
    auto grad = (cell_grad_norm.array() + neigh_grad_norm.array()) / 2.0f;
    
    result.add_fsources(interior.faces(), grad);
    result.add_fcoeffs(interior.faces(), dx, -dx);
}

#endif
