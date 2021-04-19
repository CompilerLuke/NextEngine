#pragma once

#include "cfd_core.h"
#include "numerical/spline.h"
#include "core/container/tvector.h"

struct SurfaceTriMesh;
struct CFDDebugRenderer;

struct FeatureCurves {
	tvector<Spline> splines;
	tvector<edge_handle> edges;
};

FeatureCurves identify_feature_edges(SurfaceTriMesh& surface, float feature_angle, float min_quality, CFDDebugRenderer& debug)