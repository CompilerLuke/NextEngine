//
//  simple_solver.cpp
//  CFD
//

#if 1
#include "mesh.h"
#include "solver/field.h"
#include "solver/fvm.h"
#include "solver/interpolator.h"
#include "solver/patches.h"
#include "solver/matrix.h"
#include "solver.h"

vec3 from(Eigen::Vector3d vec) { return vec3(vec[0], vec[1], vec[2]); }

struct Velocity{};
struct Pressure{};

//auto face = (w_cell.cwiseProduct(cell_values) + w_neigh.cwiseProduct(neigh_values)).cwiseProduct((w_cell + w_neigh).cwiseInverse());

using Eigen::all;

struct Simulation {
    CFDDebugRenderer& debug;
    FV_Mesh mesh;
    
    FV_Vector U_P;
    FV_Vector U;
    FV_Scalar P;
    FV_Vector H;
    FV_Scalar P_P;
    FV_Scalar VOF;

    real t = 0;

    ScalarField* inlet_pressure_grad;
    ScalarField* wall_pressure_grad;
    VectorField* wall_H;
    
    Simulation(CFDVolume& volume, CFDDebugRenderer& debug);
    void pressure_bc();
    void timestep(real dt);
    void fill_results(CFDResults& results);
};

const real rho1 = 997;// 997;
const real rho2 = 10;
const real mu1 = 1;//8.90 * 1e-4;
const real mu2 =1;// 8.90 * 1e-4; //1.48  * 1e-5;

Eigen::Array3d G = Eigen::Array3d(0, -9.81, 0);

template<typename A, typename B, typename T>
auto lerp(const A& a, const B& b, const T& t) {
    return (t-1)*a + t*b;
}

struct InterfacePressure : ScalarInterpolator {
    FV_Patch& patch;
    FaceAverage average;
    FV_Scalar& VOF;
    real P0;
    
    InterfacePressure(FaceAverage&& average, FV_Scalar& VOF, real P0) : average(std::move(average)), patch(average.interior), VOF(VOF), P0(P0) {
        
    }
    
    auto interp() const {
        auto a = 6;
        
        auto cells = VOF.values()(patch.cells());
        auto neighs = VOF.values()(patch.neighs());
        auto x = (neighs - cells).cwiseAbs()*patch.dx();
        //auto x = grad.square().colwise().sum().sqrt();
        return (10*x).max(1); //(a*x).logistic();
    }
    
    void face_values(ScalarField& result, const FV_Scalar_Data& data) const override {
        average.face_values(result, data);
        result(patch.faces()) = lerp(result(patch.faces()), P0, interp());
    }
    
    void face_coeffs(FV_ScalarMatrix& result, const FV_Scalar_Data& data) const override {
        average.face_coeffs(result, data);
        ScalarField a = interp();
        result.face_sources(patch.faces()) *= 1.0-a;
        result.face_cell_coeffs(patch.faces()) *= 1.0-a;
        result.face_neigh_coeffs(patch.faces()) *= 1.0-a;
        
        result.add_fsources(patch.faces(), a*P0);
    }
    
    void face_grad(ScalarField& result, const FV_Scalar_Data& data) const override {
        average.face_grad(result, data);
        
        auto grad = 2*(P0 - data.values(patch.cells())) * patch.dx();
        
        result(patch.faces()) = lerp(result(patch.faces()), grad, interp());
    }
    
    void face_grad_coeffs(FV_ScalarMatrix& result, const FV_Scalar_Data& data) const override {
        average.face_grad_coeffs(result, data);
        ScalarField a = interp();
        result.face_sources(patch.faces()) *= 1.0-a;
        result.face_cell_coeffs(patch.faces()) *= 1.0-a;
        result.face_neigh_coeffs(patch.faces()) *= 1.0-a;
        
        result.add_fsources(patch.faces(), 2*a*P0*patch.dx());
        result.add_fcoeffs(patch.faces(), a*0, -2*a*patch.dx());
    }
};

//todo fix leak
Simulation::Simulation(CFDVolume& volume, CFDDebugRenderer& debug) :
    mesh(build_mesh(volume, debug)), debug(debug), U_P(*mesh.data), U(*mesh.data), P(*mesh.data), H(*mesh.data), P_P(*mesh.data), VOF(*mesh.data) {
         
    auto* inlet_gradient = new FixedGradient(mesh.inlet);
    inlet_gradient->grads = 100*mesh.inlet.normal().row(2);

    assert(!inlet_gradient->grads.hasNaN());

    auto* wall_gradient = new FixedGradient(mesh.wall);
    this->wall_pressure_grad = &wall_gradient->grads;
    this->inlet_pressure_grad = &inlet_gradient->grads;

    real P0 = 1e3;
        
    //auto interface = new InterfacePressure(FaceAverage(mesh.interior), VOF, P0);
    P.add(new InterfacePressure(FaceAverage(mesh.interior), VOF, P0));
    P.add(inlet_gradient);
    P.add(wall_gradient);

    P_P.add(new InterfacePressure(FaceAverage(mesh.interior), VOF, 0));
    P_P.add(new FixedGradient(mesh.wall));
    P_P.add(new FixedGradient(mesh.inlet));

    U.add(new Upwind(mesh.interior));
    U.add(new NoSlip(mesh.wall));
    U.add(new WaveGenerator(mesh.inlet));
        
    U_P.add(new Upwind(mesh.interior));
    U_P.add(new NoSlip(mesh.wall));
    U_P.add(new WaveGenerator(mesh.inlet));

    pressure_bc();
        
    auto wall_vec = new ZeroVecGradient(mesh.wall);
    this->wall_H = &wall_vec->grads;
        
    H.add(new FaceVecAverage(mesh.interior));
    H.add(new ZeroVecGradient(mesh.wall));
    H.add(new ZeroVecGradient(mesh.inlet));
        
    VOF.add(new FaceAverage(mesh.interior));
    VOF.add(new FixedGradient(mesh.wall));
    VOF.add(new FixedGradient(mesh.inlet));
        
    auto& mesh_data = *mesh.data;
    uint cell_count = mesh_data.cell_count;
        
    auto c = mesh_data.centers();
    c(2,all) -= 2;
    VOF.values() = (100*(0.6*0.6 - c(1,all).square() - c(2,all).square() - c(0,all))).max(0).min(1);
    VOF.grads() = fvc::grad(VOF);
        
        //;.select(ScalarField::Ones(1,cell_count), ScalarField::Zero(1,cell_count));

    Eigen::initParallel();
    Eigen::setNbThreads(4);
        
    pressure_bc();

    FV_ScalarMatrix eq_P = fvm::laplace(P);
    //eq_P.set_ref(0, 10);
    eq_P.solve();
    
    P.grads() = fvc::grad(P);

    printf("Max pressure %f\n", P.values().maxCoeff());
}

//real rho = 1.0;
//real mu = 1.0;

void Simulation::pressure_bc() {
    auto t = VOF.face_values()(all, mesh.wall.faces());
    auto rho = t*rho1 + (1-t)*rho2;
    
    *wall_pressure_grad = 0;//rho*vec_dot(mesh.wall.normal(), G.replicate(1, mesh.wall.patch_count));
}

void Simulation::timestep(real dt) {
    uint cell_count = mesh.data->cell_count;
    uint face_count = mesh.data->face_count;

    real P_relax = 0.4;

    real desired_U_conv = 0.01;
    real desired_P_conv = 0.01;

    FV_Mesh_Data& mesh_data = *mesh.data;

    VectorField prev_U = U.values();
    ScalarField prev_VOF = VOF.values();

    printf("Timestep %f\n", t);

    if (t == 0) {
        t += dt;
        return;
    }
    
    uint n = 10;
    for (uint i = 0; i < n; i++) {
        //*inlet_pressure_grad = 10*mesh.inlet.normal()(2,all);
        
        auto t = VOF.values(); //(all, mesh.data.cells());
        auto t_faces = VOF.face_values();
        auto rho = (rho1*t + (1-t)*rho2).replicate<3,1>();
        auto rho_eff = (rho1*t).replicate<3,1>();
        auto mu = (t*mu1 + (1-t)*mu2).replicate<3,1>();
        auto rho_faces = (rho1*t_faces + (1-t_faces)*rho2);//.replicate<3,1>();
        
                U_P.values() = U.values();
                pressure_bc();
        
                FV_VectorMatrix eq_U = rho*fvm::ddt(U, prev_U, dt) + rho*fvm::conv(U.face_values(), U) - mu*fvm::laplace(U) == -P.grads() + rho_eff*G.replicate(1, cell_count);
                real U_conv = eq_U.solve();
        
                //2. Solve Pressure Corrector
                //H.values() = eq_U. //-rho*fvc::conv(U_P.values(), U_P);
                U_P.values() = U.values() * eq_U.A();
                FV_ScalarMatrix eq_P_corr = fvm::laplace(P_P) == fvc::div(U_P); //rho(0,all)/dt*fvc::div(U_P);
                P_P.values() = 0;
                //eq_P_corr.set_ref(0, 0)
                eq_P_corr.under_relax(P_relax);
                eq_P_corr.solve();
                
                P.values() += P_P.values();
                P.values() -= P.values().minCoeff();
                P.grads() = fvc::grad(P);
                real P_conv = (P_P.values() / P.values()).cwiseAbs().mean();
        
                //eq_U.under_relax(U_relax);
                U.values() += 1.0/eq_U.A()*fvc::grad(P_P);
                //real U_conv = (delta_U/U.values()).cwiseAbs().mean();
                U.fix_boundary();
                

        printf("Iteration %i, velocity %f, pressure %f, max velocity %f min pressure %f max pressure %f\n", i, U_conv, P_conv, U.values().cwiseAbs().maxCoeff(), P.values().minCoeff(), P.values().maxCoeff());

        if (P_conv < desired_P_conv && U_conv < desired_U_conv) break;

    }
    
    FV_ScalarMatrix eq_VOF = fvm::ddt(VOF, prev_VOF, dt);
    eq_VOF += fvm::conv(U.values(), VOF);
    eq_VOF.solve();
    
    VOF.grads() = fvc::grad(VOF);
    
    //VOF.values() = prev_VOF - dt*fvc::conv(U.values(), VOF);
    



    t += dt;
}

void Simulation::fill_results(CFDResults& result) {
    uint num_cells = mesh.data->cell_count;
    
    result.max_velocity = 0;
    result.max_pressure = 0;
    result.velocities.resize(num_cells);
    result.pressures.resize(num_cells);
    result.pressure_grad.resize(num_cells);
    result.vof.resize(num_cells);

    auto div = VOF.values(); //fvc::div(H);
    
    auto& U_values = U.values();
    auto P_values = P.values();
    auto& P_grad_values = P.grads();

    for (uint i = 0; i < num_cells; i++) {
        result.velocities[i] = from(U_values(Eigen::all, i));
        result.pressures[i] = P_values(i);
        result.pressure_grad[i] = vec3(0,0,div(i)); //from(P_grad_values(all,i));
        result.vof[i] = VOF.values()(i);
        // length(from(sim.pressure_gradients[i]));
        //sim.pressures[i];

        result.max_velocity = fmaxf(result.max_velocity, length(result.velocities[i]));
        result.max_pressure_grad = fmaxf(result.max_pressure_grad, length(result.pressure_grad[i]));
    }

    //result.max_velocity = U_values.cwiseAbs().mean();
    result.max_pressure = P_values.maxCoeff();
    result.min_pressure = P_values.minCoeff();
}

Simulation* make_simulation(CFDVolume& volume, CFDDebugRenderer& debug) {
    return new Simulation(volume, debug);
}

void destroy_simulation(Simulation* simulation) {
    delete simulation;
}

CFDResults simulate_timestep(Simulation& simulation, real dt) {
    simulation.timestep(dt);
    
    CFDResults result;
    simulation.fill_results(result);
    return result;
}

#endif
