#pragma once

#pragma once

#include "core/math/vec3.h"
#include "core/math/vec4.h"
#include "core/container/vector.h"
#include "core/memory/linear_allocator.h"
#include <lapack.h>

float* determine_c_coeff(uint n, float* h, float* y) {
	float* DL = TEMPORARY_ARRAY(float, n);
	float* D  = TEMPORARY_ARRAY(float, n+1);
	float* DU = TEMPORARY_ARRAY(float, n);
	float* B = TEMPORARY_ARRAY(float, n+1);

	for (uint i = 1; i < n; i++) {
		DL[i-1] = h[i - 1];
		D[i] = 2 * (h[i] + h[i - 1]);
		DU[i] = h[i];
		B[i] = 3 / h[i] * (y[i + 1] - y[i]) + 3 / h[i - 1] * (y[i] - y[i - 1]);
	}

	B[0] = 0;
	B[n] = 1;

	D[0] = 1;
	D[n] = 1;

	DU[0] = 0;
	DL[n - 1] = 1;

	int b_rows = n + 1;
	int b_columns = 1;
	int info;
	LAPACK_sgtsv(&b_rows, &b_columns, DL, D, DU, B, &b_rows, &info);
	assert(info == 0);

	return B;
}

//Cubic hermitian spline
class Spline {
	struct Cubic {
		vec3 a, b, c, d;
	};

	vector<Cubic> coeff;

public:
	Spline(slice<vec3> points) {
		build(points);
	}

	void build(slice<vec3> points) {
		LinearAllocator& allocator = get_temporary_allocator();
		uint n = points.length;

		coeff.resize((n - 1) * 3);

		for (uint k = 0; k < 3; k++) {
			LinearRegion region(allocator);

			float* h = TEMPORARY_ARRAY(float, n);
			float* y = TEMPORARY_ARRAY(float, n);

			for (uint i = 0; i < n; i++) {
				h[i] = 1.0;
				y[i] = points[n][k];
			}

			float* c = determine_c_coeff(n - 1, h, y);

			uint offset = (n - 1) * k;
			
			for (uint i = 0; i < n - 1; i++) {
				coeff[i].a[k] = points[i][k];
				coeff[i].b[k] = 1 / h[i] * (points[i + 1][k] - points[i][k]) - h[i] / 3 * (2 * c[i] + 2 * c[i + 1]);
				coeff[i].c[k] = c[i];
				coeff[i].d[k] = (c[i + 1] - c[i]) / (3 * h[i]);
			}
		}
	}

	vec3 interpolate(float t) {
		uint i = t;
		float x = t - i;
		float x2 = x * x;
		float x3 = x2 * x;

		vec3 result = coeff[i].a + coeff[i].b * x + coeff[i].c * x2 + coeff[i].d * x3;
		return result;
	}
};