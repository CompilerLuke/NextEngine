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
public:
    CFDDebugRenderer& debug;
	
    SurfaceTriMesh& mesh;
	SurfaceCrossField& cross_field;
    slice<FeatureCurve> curves;
    slice<float> curvatures;
		
    PointOctotree& octo;
    tvector<tri_handle> tris;

    vertex_handle add_vertex(tri_handle tri, vec3 pos);
	void propagate();
};
