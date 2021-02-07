#include "core/core.h"
#include "core/container/tvector.h"
#include "mesh.h"

struct Delaunay;
struct CFDSurface;

struct DelaunayFront {
    CFDVolume& volume;
    CFDSurface& surface;
    Delaunay& delaunay;
    
    struct VertexInfo {
        vec3 normal;
        float curvature; //+ concave, 0 flat, - convex
        uint count = 0;
    };
    
    vertex_handle active_vert_base;
    tvector<VertexInfo> active_verts;
    tvector<cell_handle> active_cells;
    
    DelaunayFront(Delaunay&, CFDSurface&);
    
    
    void generate_contour(float height);
    void generate_layer(float height);
    void generate_n_layers(uint n, float initial, float g);
    
private:
    void extrude_verts(float height);
    void update_normal(vertex_handle v, vec3 normal);
    void compute_curvature(vertex_handle v, vec3 normal);
};
