#pragma once

#include "core/container/slice.h"
#include "core/container/vector.h"

struct CFDDebugRenderer;
struct SurfaceTriMesh;
struct FeatureCurve;
struct stable_edge_handle;

vector<stable_edge_handle> remesh_surface(SurfaceTriMesh& mesh, CFDDebugRenderer&, slice<FeatureCurve> features, float uniform_mesh_size);