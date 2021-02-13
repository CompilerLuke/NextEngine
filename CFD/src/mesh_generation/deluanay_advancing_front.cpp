#include "mesh_generation/delaunay_advancing_front.h"
#include "mesh_generation/delaunay.h"

DelaunayFront::DelaunayFront(Delaunay& delaunay, CFDVolume& volume, CFDSurface& surface) : delaunay(delaunay), volume(volume), surface(surface) {
    
}

void DelaunayFront::extrude_verts(float height) {
    uint n = volume.vertices.length;
    for (uint i = active_vert_base.id; i < n; i++) {
        VertexInfo& info = active_verts[i - active_vert_base.id];
        
        vec3 p = volume.vertices[i].position;
        vec3 n = info.normal;
        float len = height * info.curvature;
        
        vertex_handle v = {(int)volume.vertices.length};
        volume.vertices.append({p + n*len});
        
        delaunay.add_vertex(v);
    }
}

void DelaunayFront::update_normal(vertex_handle v, vec3 normal) {
    if (delaunay.is_super_vert(v)) return;
    if (v.id < active_vert_base.id) return;
    
    VertexInfo& info = active_verts[v.id - active_vert_base.id];
    info.normal += normal;
    info.count++;
}

void DelaunayFront::compute_curvature(vertex_handle v, vertex_handle v2) {
    if (delaunay.is_super_vert(v)) return;
    if (v.id < active_vert_base.id) return;
    
    VertexInfo& info = active_verts[v.id - active_vert_base.id];
    
    vec3 c = volume[v].position;
    vec3 p = volume[v2].position;
    info.curvature += dot(c-p, info.normal) / info.count;
}

void DelaunayFront::generate_layer(float height) {
    //Clear
    for (uint i = 0; i < active_verts.length; i++) {
        active_verts[i] = {};
    }

    for (FaceInfo& info : active_faces) {
        CFDCell& cell = volume[info.cell];
        for (uint i = 0; i < 3; i++) {
            update_normal(cell.vertices[tetra_shape[info.face][i]], cell.faces[info.face].normal);
        }
    }

    for (FaceInfo& info : active_faces) {
        CFDCell& cell = volume[info.cell];
        for (uint i = 0; i < 3; i++) {
            uint index = tetra_shape[info.face][i];
            compute_curvature(cell.vertices[index], cell.vertices[(index+1)%3]);
        }
    }    

    extrude_verts(height);
}

void DelaunayFront::generate_contour(float height) {
    active_verts.resize(volume.vertices.length);
    
    for (CFDPolygon polygon : surface.polygons) {
        for (uint i = 0; i < polygon.type; i++) {
            vertex_handle v = polygon.vertices[i];
            update_normal(v, polygon.normal);
        }
    }
    
    for (CFDPolygon polygon : surface.polygons) {
        for (uint i = 0; i < polygon.type; i++) {
            compute_curvature(polygon.vertices[i], polygon.vertices[(i + 1) % 3]);
        }
    }
    
    active_vert_base = {0};
    for (int i = 0; i < volume.vertices.length; i++) {
        delaunay.add_vertex({i});
    }

    extrude_verts(height);
}

void DelaunayFront::generate_n_layers(uint n, float initial, float g) {
    generate_contour(initial);
    
    float h = initial;
    for (uint i = 1; i < n; i++) {
        initial *= g;
        generate_layer(h);
    }

    delaunay.complete();
}
