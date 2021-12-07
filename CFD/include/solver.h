#pragma once

#include "mesh.h"

enum CFDSolverPhase {
	SOLVER_PHASE_NONE,
	SOLVER_PHASE_MESH_GENERATION,
	SOLVER_PHASE_SIMULATION,
	SOLVER_PHASE_COMPLETE,
	SOLVER_PHASE_FAIL,
};

struct CFDResults {
	vector<vec3> velocities;
	vector<vec3> pressure_grad;
	vector<real> pressures;
    vector<real> vof;
    
    real max_velocity;
    real max_pressure;
	real min_pressure;
	real max_pressure_grad;
};

struct CFDSolver {
	CFDSolverPhase phase = SOLVER_PHASE_NONE;
	CFDVolume mesh;
	CFDResults results;
};

struct Simulation;

Simulation* make_simulation(CFDVolume& volume, CFDDebugRenderer& debug);
CFDResults simulate_timestep(Simulation& sim, real dt);

