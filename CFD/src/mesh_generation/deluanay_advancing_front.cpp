#include "mesh_generation/delaunay_advancing_front.h"
#include "mesh_generation/delaunay.h"

DelaunayFront::DelaunayFront(Delaunay& delaunay, CFDSurface& surface) : delaunay(delaunay), volume(delaunay.volume), surface(surface) {
    
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
    
    VertexInfo& info = active_verts[v.id - active_vert_base.id];
    info.normal += normal;
    info.count++;
}

void DelaunayFront::compute_curvature(vertex_handle v, vec3 normal) {
    if (delaunay.is_super_vert(v)) return;
    
    VertexInfo& info = active_verts[v.id - active_vert_base.id];
    
    vec3 c = volume[v].position;
    vec3 p = volume[cell.vertices[(index+1)%3]].position;
    info.curvature += dot(c-p, info.normal) / info.count;
}

void DelaunayFront::generate_layer(float height) {
    //Clear
    for (uint i = 0; i < active_verts.length; i++) {
        active_verts[i] = {};
    }
    
    for (uint i = 0; i < active_cells.length;) {
        cell_handle handle = active_cells[i];
        CFDCell& cell = volume[handle];
        
        bool active = true;
        for (uint i = 0; i < 4; i++) {
            vertex_handle v = cell.vertices[i];
            if (delaunay.is_super_vert(v)) continue;
            
            if (v.id < active_vert_base.id) {
                active = false;
                break;
            }
        }
        
        if (!active) {
            std::swap(active_cells.data[i], active_cells.data[--active_cells.length]);
            continue;
        }
        
        for (uint i = 0; i < 4; i++) {
            for (uint j = 0; j < 3; j++) {
                update_normal(cell.vertices[tetra_shape[i][j]], cell.faces[i].normal);
            }
        }
        
        i++;
    }
    
    for (cell_handle handle : active_cells) {
        CFDCell& cell = volume[handle];
        
        for (uint i = 0; i < 4; i++) {
            for (uint j = 0; j < 3; j++) {
                uint index = tetra_shape[i][j];
                vertex_handle v = cell.vertices[index];
                if (delaunay.is_super_vert(v)) continue;
                
                VertexInfo& info = active_verts[v.id - active_vert_base.id];
                
                vec3 c = volume[v].position;
                vec3 p = volume[cell.vertices[(index+1)%3]].position;
                info.curvature += dot(c-p, info.normal) / info.count;
            }
        }
    }
    
    extrude_verts(height);
}

void DelaunayFront::generate_contour(float height) {
    active_verts.resize(volume.vertices.length);
    
    for (CFDPolygon polygon : surface.polygons) {
        for (uint i = 0; i < polygon.type; i++) {
            vertex_handle v = polygon.vertices[i];
            
            VertexInfo& info = active_verts[v.id];
            info.normal += polygon.normal;
            info.count++;
        }
    }
    
    for (CFDPolygon polygon : surface.polygons) {
        for (uint i = 0; i < polygon.type; i++) {
            vertex_handle v = polygon.vertices[i];
            VertexInfo& info = active_verts[v.id];
            
            vec3 c = volume[v].position;
            vec3 p = volume[polygon.vertices[(i+1)%polygon.type]].position;
            info.curvature += dot(c-p, info.normal) / info.count;
        }
    }
    
    active_vert_base = {0};

    for (int i = active_vert_base.id; i < volume.vertices.length; i++) {
        delaunay.add_vertex({i});
    }
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
