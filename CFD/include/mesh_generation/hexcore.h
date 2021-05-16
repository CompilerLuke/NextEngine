#pragma once

#include "cfd_core.h"
#include "core/math/vec3.h"
#include "core/container/slice.h"
#include "core/container/vector.h"

struct Boundary;
struct SurfaceTriMesh;
struct CFDVolume;
struct PointOctotree;
struct CFDDebugRenderer;
struct AABB;

void hexcore(PointOctotree& octo, CFDVolume& volume, CFDDebugRenderer& debug);
void build_grid(CFDVolume& mesh, SurfaceTriMesh& surface, const AABB& aabb, vector<vertex_handle>& boundary_verts, vector<Boundary>& boundary, float resolution, uint layers);