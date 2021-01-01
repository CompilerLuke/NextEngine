#pragma once

#include "engine/core.h"
#include <glm/vec4.hpp>

const uint KRIGING_MAX_WEIGHTS = 12;

struct KrigingUBO {
	float a; // (sill - nugget)
	float b; // (-3.0f / range)
	int N;
	int padding;

	glm::vec4 positions[KRIGING_MAX_WEIGHTS];
	float c1[KRIGING_MAX_WEIGHTS * KRIGING_MAX_WEIGHTS];
};

ENGINE_API bool compute_kriging_matrix(KrigingUBO& kriging_ubo, uint width, uint height);

//void kriging_estimate_surface(int N_POINTS, glm::vec2* positions, float* heights, int width, int height, float* output);