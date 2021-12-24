//
//  simple_solver.cpp
//  CFD
//

#if 0
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
    
    FV_Vector U;
    FV_Scalar P;
    FV_Vector U_P;
    FV_Scalar P_P;
    FV_Vector H;

    real t = 0;

    ScalarField* inlet_pressure_grad;
    ScalarField* wall_pressure_grad;
    
    Simulation(CFDVolume& volume, CFDDebugRenderer& debug);
    void timestep(real dt);
    void fill_results(CFDResults& results);
};

Eigen::Array3d G = Eigen::Array3d(0, 0, 0);

//todo fix leak
Simulation::Simulation(CFDVolume& volume, CFDDebugRenderer& debug) :
    mesh(build_mesh(volume, debug)), debug(debug), U(*mesh.data), P(*mesh.data), U_P(*mesh.data), P_P(*mesh.data), H(*mesh.data) {
         
    auto* inlet_gradient = new FixedGradient(mesh.inlet);
    inlet_gradient->grads = 2000*mesh.inlet.normal().row(2);

    assert(!inlet_gradient->grads.hasNaN());

    auto* wall_gradient = new FixedGradient(mesh.wall);
    this->wall_pressure_grad = &wall_gradient->grads;
    this->inlet_pressure_grad = &inlet_gradient->grads;

    P.add(new FaceAverage(mesh.interior));
    P.add(inlet_gradient);
    P.add(wall_gradient);

    P_P.add(new FaceAverage(mesh.interior));
    P_P.add(wall_gradient);
    P_P.add(new FixedGradient(mesh.inlet));

    U.add(new Upwind(mesh.interior));
    U.add(new NoSlip(mesh.wall));
    U.add(new WaveGenerator(mesh.inlet));

    H.add(new FaceVecAverage(mesh.interior));
    H.add(new ZeroVecGradient(mesh.wall));
    H.add(new ZeroVecGradient(mesh.inlet));

    U_P.add(new Upwind(mesh.interior));
    U_P.add(new NoSlip(mesh.wall));
    U_P.add(new WaveGenerator(mesh.inlet));
    
    *wall_pressure_grad =  vec_dot(mesh.wall.normal(), G.replicate(1, mesh.wall.patch_count));

    Eigen::initParallel();
    Eigen::setNbThreads(4);

    FV_ScalarMatrix eq_P = fvm::laplace(P);
    eq_P.set_ref(0, 10);
    eq_P.solve();


    P.grads() = fvc::grad(P);
}

void Simulation::timestep(real dt) {
    uint cell_count = mesh.data->cell_count;
    uint face_count = mesh.data->face_count;


    real rho = 1.0;
    real mu = 1.0;
    
    real U_relax = 0.4;
    real P_relax = 0.4;

    real desired_U_conv = 0.01;
    real desired_P_conv = 0.01;

    FV_Mesh_Data& mesh_data = *mesh.data;

    VectorField prev_U = U.values();

    printf("Timestep %f\n", t);
    
    uint n = 100;
    for (uint i = 0; i < n; i++) {
        *inlet_pressure_grad = 2000*mesh.inlet.normal()(2,all);

        auto fluxes = U.face_values();
        U_P.values() = U.values();
        //-1/rho*P.grads()

        FV_VectorMatrix eq_U = fvm::ddt(U_P,prev_U,dt) + fvm::conv(fluxes, U_P) -mu * fvm::laplace(U_P) == -1 / rho * P.grads() + 1.0/rho*G.replicate(1, cell_count);
        eq_U.under_relax(U_relax);
        real U_conv = eq_U.solve();

        //AU - H = -\P
        //U - A*H = -A*\P
        //U = A*H - A*\P
        //H = AU + \P

#if 0
        VectorField inv_A = eq_U.A().cwiseInverse();
        ScalarField inv_A_faces = inv_A(all,mesh_data.cells())(0,all);
            //vec_dot(inv_A(all,mesh_data.cells()), mesh_data.normal);
        
        H.values() = eq_U.A() * U.values() + P.grads();
        //H.grads() = fvc::grad(H);

//U = 0, \P = -H
#endif
#if 1
        H.values() = -fvc::conv(U.face_values(), U);
#endif

        //*wall_pressure_grad = -vec_dot(H(all, mesh.wall.cells()), mesh.wall.normal());
           //vec_dot(mesh.wall.normal(), G.replicate(1, mesh.wall.patch_count));

        FV_ScalarMatrix eq_P = fvm::laplace(P) == rho / dt * fvc::div(U_P);
        eq_P.under_relax(P_relax);
        //eq_P.set_ref(0, 10);

        real P_conv = eq_P.solve();        
        printf("Min pressure %f\n", P.values().minCoeff());
        P.values() += 10-P.values().minCoeff();
        printf("Min pressure %f\n", P.values().minCoeff());
        
        P.grads() = fvc::grad(P);

        //Corrector
        U.values() = U_P.values();// -dt / rho * P.grads();

        printf("Iteration %i, velocity %f, pressure %f, max velocity %f\n", i, U_conv, P_conv, U.values().maxCoeff());

        if (P_conv < desired_P_conv && U_conv < desired_U_conv) break;
    }

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

    auto& U_values = U.values();
    auto& P_values = P.values();
    auto& P_grad_values = P.grads();

    for (uint i = 0; i < num_cells; i++) {
        result.velocities[i] = from(U_values(Eigen::all, i));
        result.pressures[i] = P_values(i);
        result.pressure_grad[i] = from(P_grad_values(all,i));
        // length(from(sim.pressure_gradients[i]));
        //sim.pressures[i];

        result.max_velocity = fmaxf(result.max_velocity, length(result.velocities[i]));  
        result.max_pressure_grad = fmaxf(result.max_pressure_grad, length(result.pressure_grad[i]));  
    }

    result.max_velocity = U_values.cwiseAbs().mean();
    result.max_pressure = P_values.maxCoeff();
    result.min_pressure = P_values.minCoeff();
}

Simulation* make_simulation(CFDVolume& volume, CFDDebugRenderer& debug) {
    return new Simulation(volume, debug);
}



CFDResults simulate_timestep(Simulation& simulation, real dt) {
    simulation.timestep(dt);
    
    CFDResults result;
    simulation.fill_results(result);
    return result;
}

#endif
