#pragma once

#pragma once

#include "core/math/vec3.h"
#include "core/math/vec4.h"
#include "core/container/vector.h"
#include "core/memory/linear_allocator.h"
#include <lapacke.h>

/*
void compute_spline_coeff(uint n, float* coeff, float* y, uint stride, float* h, float constant_h) {
	LinearRegion region(get_temporary_allocator());
	
	float* DL = TEMPORARY_ARRAY(float, n);
	float* D  = TEMPORARY_ARRAY(float, n+1);
	float* DU = TEMPORARY_ARRAY(float, n);
	float* B = TEMPORARY_ARRAY(float, n + 1);

	for (uint i = 1; i < n; i++) {
		float h0 = h ? h[i - 1] : constant_h;
		float h1 = h ? h[i] : constant_h;

		float a0 = y[(i - 1) * stride];
		float a1 = y[i * stride];
		float a2 = y[(i + 1) * stride];

		DL[i-1] = h0;
		D[i] = 2*(h0 + h1);
		DU[i] = h1;
		B[i] = 3/h1 * (a2 - a1) + 3/h0 * (a1 - a0);
	}

	B[0] = 0;
	B[n] = 0;

	D[0] = 1;
	D[n] = 1;

	DU[0] = 0;
	DL[n - 1] = 1;
	
	int info = LAPACKE_sgtsv(LAPACK_ROW_MAJOR, n+1, 1, DL, D, DU, B, 1);
	assert(info == 0);

	for (uint i = 0; i < n; i++) {
		float h0 = h ? h[i] : constant_h;

		float a0 = y[i * stride];
		float a1 = y[(i + 1) * stride];

		float c0 = B[i];
		float c1 = B[i + 1];

		coeff[i * 4 * stride + 0] = a0;
		coeff[i * 4 * stride + 1] = 1 / h0 * (a1 - a0) - h0 / 3 * (2 * c0 + c1);
		coeff[i * 4 + stride + 2] = c0;
		coeff[i * 4 + stride + 3] = (c1 - c0) / (3*h0);
	}
}
*/

//Cubic hermitian spline
class Spline {
	struct Cubic {
		vec3 a, b, c, d;
	};

	vector<Cubic> coeff;
	vector<float> lengths;

public:
	Spline(slice<vec3> points) {
		build(points);
	}

	void build(slice<vec3> points) {
		uint n = points.length - 1;

		coeff.resize(n);
		lengths.resize(n);

		LinearRegion region(get_temporary_allocator());

		float* DL = TEMPORARY_ARRAY(float, n);
		float* D = TEMPORARY_ARRAY(float, n + 1);
		float* DU = TEMPORARY_ARRAY(float, n);
		float* B = TEMPORARY_ARRAY(float, n + 1);

		for (uint k = 0; k < 3; k++) {
			for (uint i = 1; i < n; i++) {
				float a0 = points[i - 1][k];
				float a1 = points[i][k];
				float a2 = points[i + 1][k];

				DL[i - 1] = 1.0;
				D[i] = 2;
				DU[i] = 1.0;
				B[i] = 3.0*(a2 - a1) + 3.0*(a1 - a0);
			}

			B[0] = 0; B[n] = 0;
			D[0] = 1; D[n] = 1;

			DL[n - 1] = 1;
			DU[0] = 0;

			int info = LAPACKE_sgtsv(LAPACK_ROW_MAJOR, n + 1, 1, DL, D, DU, B, 1);
			assert(info == 0);

			for (uint i = 0; i < n; i++) {
				float a0 = points[i][k]; 
				float a1 = points[i + 1][k];

				float c0 = B[i];
				float c1 = B[i + 1];

				coeff[i].a[k] = a0;
				coeff[i].b[k] = (a1 - a0) - 1.0 / 3 * (2 * c0 + c1);
				coeff[i].c[k] = c0;
				coeff[i].d[k] = (c1 - c0) / 3.0;
			}
		}

		for (uint i = 0; i < n; i++) {
			lengths[i] = arc_length_integrand(i,1.0);
		}
	}

	vec3 at_time(float t) {
		uint i = t;
		float x = t - i;
		float xx = x * x;
		float xxx = xx * x;

		vec3 result = coeff[i].a + coeff[i].b * x + coeff[i].c * xx + coeff[i].d * xxx;
		return result;
	}

	float arc_length_integrand(uint spline, float t) {
		float tt = t * t;

		vec3 result = coeff[spline].b + 2.0f * coeff[spline].c * t + 3.0f * coeff[spline].d * tt;
		return length(result);
	}

	float get_total_length() {
		float total = 0.0f;
		for (uint i = 0; i < lengths.length; i++) {
			total += lengths[i];
		}
		return total;
	}

	float integrate(uint spline, float t) {
		uint n = 16;
		float h = t / n;
		
		float XI0 = arc_length_integrand(spline, t);
		float XI1 = 0;
		float XI2 = 0;

		for (uint i = 0; i < n; i++) {
			float X = i * h;
			if (i % 2 == 0) XI2 += arc_length_integrand(spline, X);
			else XI1 += arc_length_integrand(spline, X);
		}

		float XI = h * (XI0 + 2 * XI2 + 4 * XI1) / 3.0f;
		return XI;
	}

	vec3 constant_speed_at_time(float t, float speed) {
		float total_length = get_total_length();
		float desired_distance = speed * t;

		float distance = 0.0f;

		uint spline;
		for (spline = 0; spline < lengths.length; spline++) {
			distance += integrate(spline, 1.0);
			if (distance > desired_distance) break;
		}

		float s = 0.0f;
		for (uint i = 0; i < 6; i++) {
			s = (integrate(spline, s) - t) / arc_length_integrand(spline, s);
		}

		return at_time(spline + s);
	}
};