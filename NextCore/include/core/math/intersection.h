#pragma once

#include "core/math/vec3.h"
#include "core/math/aabb.h"

struct Ray {
	glm::vec3 orig, dir;
	glm::vec3 invdir;
	float t;

	uint sign[3];

	inline Ray(glm::vec3 o, glm::vec3 d, float t = FLT_MAX) : orig(o), dir(d), t(t) {
		invdir = 1.0f / d;
		sign[0] = invdir.x < 0;
		sign[1] = invdir.y < 0;
		sign[2] = invdir.z < 0;
	}
};

inline bool ray_triangle_intersect(const Ray& ray, vec3 p[3], float* t) {
    vec3 orig = ray.orig;
    vec3 dir = ray.dir;
    float max_t = ray.t;

    vec3 v0v1 = p[1] - p[0];
    vec3 v0v2 = p[2] - p[0];

    vec3 pvec = cross(dir, v0v2);

    float det = dot(v0v1, pvec);

    if (fabs(det) < FLT_EPSILON) return false;

    float inv_det = 1.0 / det;

    vec3 tvec = orig - p[0];

    float u = dot(tvec, pvec) * inv_det;

    if (u < 0 || u > 1) return false;

    vec3 qvec = cross(tvec, v0v1);

    float v = dot(dir, qvec) * inv_det;

    if (v < 0 || u + v > 1)
        return false;

    *t = dot(v0v2, qvec) * inv_det;
    return *t > 0 && *t < max_t;
}

//Math from - https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
inline bool ray_aabb_intersect(const AABB& bounds, const Ray& r, float* t = nullptr, bool allow_inside = true) {
	float tmin, tmax, tymin, tymax, tzmin, tzmax;
	tmin = (bounds[r.sign[0]].x - r.orig.x) * r.invdir.x;
	tmax = (bounds[1 - r.sign[0]].x - r.orig.x) * r.invdir.x;
	tymin = (bounds[r.sign[1]].y - r.orig.y) * r.invdir.y;
	tymax = (bounds[1 - r.sign[1]].y - r.orig.y) * r.invdir.y;

	if ((tmin > tymax) || (tymin > tmax)) return false;
	if (tymin > tmin) tmin = tymin;
	if (tymax < tmax) tmax = tymax;

	tzmin = (bounds[r.sign[2]].z - r.orig.z) * r.invdir.z;
	tzmax = (bounds[1 - r.sign[2]].z - r.orig.z) * r.invdir.z;

	if ((tmin > tzmax) || (tzmin > tmax)) return false;
	if (tzmin > tmin) tmin = tzmin;
	if (tzmax < tmax) tmax = tzmax;

	if (t) *t = tmin;
	return (allow_inside || tmin > 0) && tmin < r.t;
}
