#pragma once

#include "core/container/slice.h"
#include "core/container/vector.h"
#include "core/container/tvector.h"
#include "core/math/vec3.h"

struct SurfaceTriMesh;
struct SurfaceCrossField;
struct PointOctotree;
struct InputModelBVH;
struct Spline;
struct CFDDebugRenderer;
struct FeatureCurve;

class SurfacePointPlacement {
	SurfaceTriMesh& mesh;
	SurfaceCrossField& cross_field;
	PointOctotree& octo;
    slice<float> curvatures;
	tvector<vec3>& shadow_points;

public:
	SurfacePointPlacement(SurfaceTriMesh&, SurfaceCrossField&, PointOctotree&, tvector<vec3>& shadow_points, slice<float> curvatures);

	void propagate(slice<FeatureCurve> curves, CFDDebugRenderer& debug);
};
