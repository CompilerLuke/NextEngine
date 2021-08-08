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
	vector<real> pressures;
};

struct CFDSolver {
	CFDSolverPhase phase = SOLVER_PHASE_NONE;
	CFDVolume mesh;
	CFDResults results;
};

CFDResults simulate(CFDVolume& volume, CFDDebugRenderer& debug);

