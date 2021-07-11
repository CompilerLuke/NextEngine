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
    float total_length;
    
	Spline(slice<vec3> points) {
		build(points);
	}
    
    uint points() {
        return lengths.length;
    }

	void build(slice<vec3> points) {
		uint n = points.length - 1;

		coeff.resize(n+1);
		lengths.resize(n);

		LinearRegion region(get_temporary_allocator());

		vec3* a = TEMPORARY_ARRAY(vec3, n + 1);
		for (int i = 1; i <= n - 1; i++)
			a[i] = 3 * ((points[i + 1] - 2 * points[i] + points[i - 1]));

		float* l = TEMPORARY_ARRAY(float, n+1);
		float* mu = TEMPORARY_ARRAY(float, n + 1); 
		vec3* z = TEMPORARY_ARRAY(vec3, n + 1);

		l[0] = l[n] = 1;
		mu[0] = 0;
		z[0] = z[n] = vec3();
		coeff[n].c = vec3();

		for (int i = 1; i <= n - 1; i++)
		{
			l[i] = 4 - mu[i - 1];
			mu[i] = 1 / l[i];
			z[i] = (a[i] - z[i - 1]) / l[i];
		}

		for (int i = 0; i < n+1; i++)
			coeff[i].a = points[i];

		for (int j = n - 1; j >= 0; j--)
		{
			coeff[j].c = z[j] - mu[j] * coeff[j + 1].c;
			coeff[j].d = (1.0f / 3.0f) * (coeff[j + 1].c - coeff[j].c);
			coeff[j].b = points[j + 1] - points[j] - coeff[j].c - coeff[j].d;
		}

		/*		
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
				D[i] = 4.0;
				DU[i] = 1.0;
				B[i] = 3.0*(a2 - a1) + 3.0*(a1 - a0);
			}

			B[0] = 0; B[n] = 0;
			D[0] = 1; D[n] = 1;

			DL[n - 1] = 0;
			DU[0] = 0;
            
            if (true) {
                char spaces[256];
                for (uint i = 0; i < 256; i++) spaces[i] = ' ';
                
                for (uint i = 0; i < n+1; i++) {
                    if (i == 0) printf("[ %.2f, %.2f,%.*s][%i] = [%f]\n", D[i], DU[0], (n-1)*6, spaces, i, B[i]);
                    else if (i == n) printf("[%.*s %.2f, %.2f ][%i] = [%f]\n", (i-1)*6, spaces, DL[i-1], D[i], i, B[i]);
                    else printf("[%.*s %.2f, %.2f, %.2f,%.*s][%i] = [%f]\n", (i-1)*6, spaces, DL[i-1], D[i], DU[i], (n-1-i)*6, spaces, i, B[i]);
                }
            }
            
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
		*/

        total_length = 0;
		for (uint i = 0; i < n; i++) {
			lengths[i] = integrate(i,1.0);
            total_length += lengths[i];
		}
	}

	vec3 at_time(float t) {
		uint i = min(t, coeff.length-1);
		float x = t - i;
		float xx = x * x;
		float xxx = xx * x;

		vec3 result = coeff[i].a + coeff[i].b * x + coeff[i].c * xx + coeff[i].d * xxx;
		return result;
	}
    
    vec3 tangent(float t) {
        uint i = min(t, coeff.length - 1);
        float x = t - i;
        float xx = x * x;
        
        vec3 result = coeff[i].b + 2.0f * coeff[i].c * x + 3.0f * coeff[i].d * xx;
        return normalize(result);
    }

	float arc_length_integrand(uint spline, float t) {
		float tt = t * t;

		vec3 result = coeff[spline].b + 2.0f * coeff[spline].c * t + 3.0f * coeff[spline].d * tt;
		return length(result);
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
    
    float const_speed_reparam(float t, float speed) {
        float desired_distance = speed * t;

        float distance = 0.0f;

        uint spline;
        for (spline = 0; spline < lengths.length; spline++) {
            float new_distance = distance + lengths[spline];
            if (new_distance >= desired_distance) break;
			distance = new_distance;
        }
        
        assert(spline < lengths.length);
        float d = desired_distance - distance;

        float s = 0.5f;
        for (uint i = 0; i < 10; i++) {
            float arc_length = integrate(spline, s);
            float arc_speed = arc_length_integrand(spline, s);
            //printf("S %f, arc length %f, arc speed %f\n", s, arc_length, arc_speed);
            s = s - (arc_length - d) / arc_speed;
        }
        
        return spline + s;
    }

	vec3 const_speed_at_time(float t, float speed) {
		return at_time(const_speed_reparam(t, speed));
	}
};
