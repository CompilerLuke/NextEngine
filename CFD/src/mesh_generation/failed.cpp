
/*
float min_t = 0.0f;
do {
    CFDCell& cell = volume.cells[current.id];

    if (point_inside(circumspheres[current.id], pos)) break;

    cell_handle last = current;
    current = {};

    compute_normals(volume.vertices, cell);

    float smallest_t = FLT_MAX;
    for (uint i = 0; i < 4; i++) {
        cell_handle neighbor = cell.faces[i].neighbor;
        if (neighbor.id == -1 || neighbor.id == prev.id) continue;

        vec3 v0 = volume.vertices[cell.vertices[tetra_shape.faces[i].verts[0]].id].position;
        vec3 n = cell.faces[i].normal;
        float denom = dot(n, dir);
        if (fabs(denom) < 1e-6) continue;

        float t = dot(n, v0 - orig) / denom;

        printf("n: %f, %f, %f\n", n.x, n.y, n.z);
        printf("t: %f, last t: %f, min t: %f, max t: %f\n", t, smallest_t, min_t, max_t);
        if (t > min_t && t < max_t && t < smallest_t) {
            smallest_t = t;
            current = neighbor;
            printf("Setting current %i, prev %i\n", current.id, prev.id);
        }
    }

    if (current.id != -1) {
        add_box_cell(volume, orig + smallest_t * dir, 0.1);
        printf("%i -> %i, t: %f, max: %f\n", last.id, current.id, smallest_t, max_t);
    }

    prev = last;
    min_t = smallest_t;
} while (current.id != -1);
*/


/*

cavity.clear();
shared_face.capacity = 10000;
shared_face.clear();

for (int i = super_offset; i < volume.cells.length; i++) {
    if (!point_inside(circumspheres[i], pos)) continue;
    CFDCell& cell = volume.cells[i];
    circumspheres[i].radius = 0.0f;

    for (uint i = 0; i < 4; i++) {
        vertex_handle verts[3];
        for (uint j = 0; j < 3; j++) {
            verts[j] = cell.vertices[tetra_shape.faces[i].verts[2 - j]];
        }

        FaceInfo& info = shared_face[verts];
        if (info.face == -1) {
            info.face = 0;
            info.neighbor = cell.faces[i].neighbor;
        }
        else {
            info.face = 1;
        }
    }

    free_cells.append({ i });
}

for (uint i = 0; i < shared_face.capacity; i++) {
    FaceInfo& info = shared_face.values[i];

    if (info.face == 0) {
        CavityFace face;
        face.handle = info.neighbor;
        memcpy_t(face.verts, shared_face.keys[i].verts, 3);

        cavity.append(face);
    }
}
*/
