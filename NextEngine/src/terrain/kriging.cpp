//
//  kriging.cpp
//  C_ECS
//
//  Created by Lucas  Goetz on 10/07/2020.
//  Copyright © 2020 Lucas  Goetz. All rights reserved.
//

#include <stdio.h>
#include <math.h>
#include <glm/vec2.hpp>
#include <glm/glm.hpp>
#include "core/container/slice.h"
#include "core/memory/linear_allocator.h"
#include <string.h>
#include "core/profiler.h"
#include "terrain/kriging.h"

struct Point {
	glm::vec2 position;
	float height;
};

struct Matrix {
	uint N;
	float* values;

	float* operator[](uint row) {
		return values + row * N;
	}
};

/*struct Variogram {
	float nugget;
	float sill;
	float range;
};*/

float cov(float nugget, float sill, float range, float h) {
	return (sill - nugget) * expf(-3.0f*h / range);
}

void lu(Matrix mat, Matrix lower, Matrix upper) {
	int N = mat.N;
	int i = 0, j = 0, k = 0;
	for (i = 0; i < N; i++) {
		for (j = 0; j < N; j++) {
			if (j < i) {
				lower[j][i] = 0;
			}
			else {
				lower[j][i] = mat[j][i];
				for (k = 0; k < i; k++) {
					lower[j][i] = lower[j][i] - lower[j][k] * upper[k][i];
				}
			}
		}
		for (j = 0; j < N; j++) {
			if (j < i) {
				upper[i][j] = 0;
			}
			else if (j == i) {
				upper[i][j] = 1;
			}
			else {
				upper[i][j] = mat[i][j] / lower[i][i];
				for (k = 0; k < i; k++) {
					upper[i][j] = upper[i][j] - ((lower[i][k] * upper[k][j]) / lower[i][i]);
				}
			}
		}
	}
}

void output_float(float value) {
	printf("%-6.2f ", value);
}

void output_vec(slice<float> rows) {
	printf("[ ");
	for (float row : rows) {
		output_float(row);
	}
	printf("] \n");
}

void output_mat(Matrix matrix) {
	int N = matrix.N;

	for (int i = 0; i < N; i++) {
		printf("[ ");
		for (int j = 0; j < N; j++) {
			output_float(matrix[i][j]);
		}

		printf("] \n");
	}
}

float compute_z(int col, int row, Matrix lower, Matrix Z, Matrix I) {
	int N = I.N;

	float sum = 0;
	for (int i = 0; i < N; i++) {
		if (i != row) {
			sum += lower[row][i] * Z[i][col];
		}
	}

	float result = I[row][col] - sum;
	result = result / lower[row][row];

	return result;
}

float compute_inverse(int col, int row, Matrix inverse, Matrix upper, Matrix Z) {
	int N = inverse.N;

	float sum = 0;
	for (int i = 0; i < N; i++) {
		if (i != row) {
			sum += upper[row][i] * inverse[i][col];
		}
	}

	float result = Z[row][col] - sum;
	result = result / upper[row][row];


	return result;
}

void inverse_matrix(Matrix mat, Matrix inverse, Matrix upper, Matrix lower, Matrix Z, Matrix I) {
	int N = mat.N;

	lu(mat, lower, upper);

	// compute z
	for (int col = 0; col < N; col++) {
		for (int row = 0; row < N; row++) {
			Z[row][col] = compute_z(col, row, lower, Z, I);
		}
	}


	// compute inverse
	for (int col = 0; col < N; col++) {
		for (int row = N - 1; row >= 0; row--) {
			inverse[row][col] = compute_inverse(col, row, inverse, upper, Z);
		}
	}

}

void mul_matrix(Matrix mat, float* vec, float* result) {
	for (int i = 0; i < mat.N; i++) {
		float sum = 0.0f;
		for (int j = 0; j < mat.N; j++) {
			sum += mat[i][j] * vec[j];
		}

		result[i] = sum;
	}
}


Matrix alloc_temporary_matrix(uint N) {
	return { N, TEMPORARY_ZEROED_ARRAY(float, N*N) };
}



bool compute_kriging_matrix(KrigingUBO& kriging_ubo, uint width, uint height) {
	const uint N_POINTS = kriging_ubo.N;
	const glm::vec4* positions = kriging_ubo.positions;

	Profile kriging("Kriging");

	float max_dist = ceilf(sqrt(width*width + height * height));

	int bin_width = max_dist / 20;
	int max_bin = 21;

	/*
	//COMPUTE VARIOGRAM
	float* variance_bins = TEMPORARY_ZEROED_ARRAY(float, max_bin, 0);
	int* variance_bins_count = TEMPORARY_ZEROED_ARRAY(int, max_bin, 0);

	for (int i = 0; i < N_POINTS; i++) {
		for (int j = i + 1; j < N_POINTS; j++) {
			float dist = glm::length(positions[i] - positions[j]);

			int index = dist / bin_width;
			assert(index < max_bin);

			variance_bins[index] += powf(positions[i].z - positions[j].z, 2);
			variance_bins_count[index]++;

			//printf("Semi variance %f index %i\n", variance_bins[index], index);
		}
	}

	float sill = 0.0;
	float range = 0.0;

	for (int i = 0; i < max_bin; i++) {
		int count = variance_bins_count[i];
		if (count == 0) continue;

		float semivariance = 0.5f * variance_bins[i] / count;

		variance_bins[i] = semivariance;

		if (semivariance > sill) {
			sill = semivariance;
			range = i;
		}
	}

	if (range == 0) {
		printf("No control points set!");

		return false;
	}

	float nugget = variance_bins[0];

	range *= bin_width;

	//printf("Nugget %f\nSill %f\nRange %f\n", nugget, sill, range);
	*/ 

	float radius = max_dist * 0.5f;
	float nugget = 0.0f;
	float sill = 0.5f * powf(radius, 2);
	float range = radius;

	int N = ++kriging_ubo.N;

	//ALLOCATE MATRICES
	Matrix c1 = alloc_temporary_matrix(N); //todo remove with static array allocations
	Matrix c1_inv = { N, kriging_ubo.c1 };
	Matrix I = alloc_temporary_matrix(N);
	Matrix Z = alloc_temporary_matrix(N);
	Matrix lower = alloc_temporary_matrix(N);
	Matrix upper = alloc_temporary_matrix(N);

	//printf("Allocated matrices!\n");

	//COMPUTE COVARIANCE MATRIX
	for (int i = 0; i < N; i++) {
		I[i][i] = 1.0;
	}

	for (int i = 0; i < N_POINTS; i++) {
		c1[i][N - 1] = 1;
		c1[N - 1][i] = 1;
	}

	for (int i = 0; i < N_POINTS; i++) {
		for (int j = i; j < N_POINTS; j++) {
			float dist = glm::length(positions[i] - positions[j]);
			c1[i][j] = cov(nugget, sill, range, dist);
		}
	}

	for (int i = 0; i < N_POINTS; i++) {
		for (int j = 0; j < i; j++) {
			c1[i][j] = c1[j][i];
		}
	}

	//printf("=== C1 ===\n");
	//output_mat(c1);

	inverse_matrix(c1, c1_inv, upper, lower, Z, I);

	//printf("=== C1 INV ===\n");
	//output_mat(c1_inv);

	
	kriging_ubo.a = (sill - nugget);
	kriging_ubo.b = -3.0f / range;

	kriging.end();

	return true;


	//if (total_weight > 1.1) {
	//	printf("total weight %f\n", total_weight);
	//}
	//
}

void cpu_estimate_terrain_surface(uint width, uint height) {
	/*
	float* c0 = TEMPORARY_ARRAY(float, N);
	float* weights = TEMPORARY_ARRAY(float, N);
	c0[N - 1] = 1;

	
	Profile surface("Terrain surface generation");

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			for (int i = 0; i < N_POINTS; i++) {
				float dist = glm::length(positions[i] - glm::vec2(x, y));
				c0[i] = cov(nugget, sill, range, dist);
			}

			mul_matrix(c1_inv, c0, weights);

			float weighted_sum = 0.0f;
			for (int i = 0; i < N_POINTS; i++) {
				weighted_sum += weights[i] * heights[i];
			}

			output[x + height * y] = weighted_sum;
		}
	}

	surface.end();
	*/
}
