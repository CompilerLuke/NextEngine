#pragma once

#include "core/container/slice.h"

struct SurfaceTriMesh;
struct CFDDebugRenderer;
struct SurfaceCrossField;
struct stable_edge_handle;

void qmorph(SurfaceTriMesh& mesh, CFDDebugRenderer& debug, SurfaceCrossField& cross_field, slice<stable_edge_handle> edges, real mesh_size);
