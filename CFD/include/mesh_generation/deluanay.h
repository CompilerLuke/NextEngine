#pragma once

#include "core/container/tvector.h"
#include "core/container/hash_map.h"
#include "mesh.h"
#include "graphics/culling/aabb.h"

struct FaceInfo {
    int face = -1;
    cell_handle neighbor;
};

struct CavityFace {
    cell_handle handle;
    vertex_handle verts[3];
};

struct Deluanay {
    CFDVolume& volume;
    hash_map_base<TriangleFaceSet, FaceInfo> shared_face;
    AABB aabb;
    uint max_shared_face;
    uint super_offset;
    tvector<CavityFace> cavity;
    tvector<vec4> subdets;
    tvector<cell_handle> stack;
    tvector<cell_handle> free_cells;
    cell_handle last;
    vertex_handle super_vertex_base;
    
    Deluanay(CFDVolume&, const AABB& aabb);
    bool add_vertices(slice<vertex_handle> verts);
    bool find_in_circum(vec3 pos, vertex_handle v);
    bool add_face(const Boundary& boundary);
    bool add_vertex(vertex_handle vert);
    bool complete();
    
private:
    int get_face_index(const TriangleFaceSet& face, CFDCell& neighbor);
    bool update_circum(cell_handle cell_handle);
    
};

