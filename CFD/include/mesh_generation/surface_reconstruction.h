#pragma once

#include "core/container/slice.h"

struct vec3;
struct SurfaceTriMesh;
struct CFDDebugRenderer;

using tri_handle = uint;

void reconstruct_surface(SurfaceTriMesh& surface, CFDDebugRenderer& debug, slice<FeatureCurve> features);