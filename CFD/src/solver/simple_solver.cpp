//
//  simple_solver.cpp
//  CFD
//
//  Created by Antonella Calvia on 12/10/2021.
//

#if 1
#include "mesh.h"
#include "solver/finite_volume.h"
#include "solver/solver_abstract.h"
#include "solver/evaluator.h"

struct Velocity{};
struct Pressure{};

struct FV_VP_BFace
: FV_BFace, FV_Diri_B<Velocity>, FV_Diri_B<Pressure> {
    
};

void solve() {
    ScalarUnknown<Velocity> U;
    vector<real> P = vector<real>();
    
    real mu = 1.0;
    real rho = 1.0;
    
    //Momentum Equation
    FV_Mesh<FV_IFace, FV_VP_BFace> mesh;
    
    eval_on_mesh(mesh, laplacian(U));
    //solve_on_mesh(mesh, laplacian(U), grad(P));
}

/*void make_simulation(CFDVolume& volume, CFDDebugRenderer& debug) {
    
}

void simulate_timestep(struct Simulation& simulation, real dt) {
    
}*/

#endif
