#pragma once

#include "core/container/slice.h"
#include "core/container/vector.h"
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

public:
	SurfacePointPlacement(SurfaceTriMesh&, SurfaceCrossField&, PointOctotree&, slice<float> curvatures);

	void propagate(slice<FeatureCurve> curves, CFDDebugRenderer& debug);
};
