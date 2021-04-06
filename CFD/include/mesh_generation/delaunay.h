#pragma once

#include "core/math/vec4.h"

using tet_handle = uint;

const tet_handle NEIGH = ((1 << 30)-1) << 2;
const tet_handle NEIGH_FACE = (1<<2)-1;

struct TetMesh {
    uint tet_count;
    vertex_handle* indices;
    tet_handle* faces;
};

struct Delaunay;
struct CFDVolume;
struct CFDSurface;

Delaunay* make_Delaunay(CFDVolume&, const AABB&);
void destroy_Delaunay(Delaunay*);
void generate_n_layers(Delaunay& d, CFDSurface& surface, uint n, float initial, float g);
TetMesh generate_uniform_tri_mesh(Delaunay& d, CFDSurface& surface, float size);
bool add_vertices(Delaunay& d, slice<vertex_handle> verts, float min_dist = 0.0f);
