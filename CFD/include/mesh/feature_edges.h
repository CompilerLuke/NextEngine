#pragma once

#include "cfd_core.h"
#include "numerical/spline.h"
#include "core/container/tvector.h"

struct SurfaceTriMesh;
struct CFDDebugRenderer;
struct EdgeGraph;

struct FeatureCurve {
    Spline spline;
    tvector<edge_handle> edges;
};

tvector<float> curvature_at_verts(SurfaceTriMesh& surface, EdgeGraph& graph, CFDDebugRenderer& debug);
tvector<FeatureCurve> identify_feature_edges(SurfaceTriMesh& surface, EdgeGraph& graph, float feature_angle, float min_quality, CFDDebugRenderer& debug);
