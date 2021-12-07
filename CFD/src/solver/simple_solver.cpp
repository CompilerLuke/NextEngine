//
//  simple_solver.cpp
//  CFD
//
//  Created by Antonella Calvia on 12/10/2021.
//

#if 1
#include "mesh.h"
#include "solver/field.h"
#include "solver/fvm.h"
#include "solver/interpolator.h"
#include "solver/fmesh.h"

struct Velocity{};
struct Pressure{};

//auto face = (w_cell.cwiseProduct(cell_values) + w_neigh.cwiseProduct(neigh_values)).cwiseProduct((w_cell + w_neigh).cwiseInverse());


struct Simulation {
    CFDDebugRenderer& debug;
    FV_Mesh mesh;
    
    FV_Vector U;
    FV_Scalar P;
    
    Simulation(CFDVolume& volume, CFDDebugRenderer& debug);
    void timestep(real dt);
};

Simulation::Simulation(CFDVolume& volume, CFDDebugRenderer& debug) :
    mesh(build_mesh(volume)), debug(debug), U(*mesh.data), P(*mesh.data) {
    
    ScalarField inlet;
        
    P.add(new FaceAverage(mesh.interior));
    P.add(new FixedGradient(mesh.inlet, std::move(inlet)));
}

void Simulation::timestep(real dt) {
    FV_ScalarMatrix eqP = fvm::laplace(P);
    eqP.solve();
}

Simulation* make_simulation(CFDVolume& volume, CFDDebugRenderer& debug) {
    return new Simulation(volume, debug);
}

void simulate_timestep(Simulation& simulation, real dt) {
    simulation.timestep(dt);
}

#endif
