#include "mesh_generation/front_octotree.h"
#include "core/math/vec3.h"
#include "core/math/intersection.h"
#include "core/memory/linear_allocator.h"

Front::Front(vector<CFDVertex>& vertices, vector<CFDCell>& cells, const AABB& aabb)
    : vertices(vertices), cells(cells) {
    root = {};
    free_payload = nullptr;
    init(root);
    root.aabb_division = aabb;
    last_visited = &root; //note: self-referential so moving this struct will cause problems!
}

bool Front::is_leaf(Subdivision& subdivision) {
    return subdivision.count <= MAX_PER_CELL;
}

void Front::init(Subdivision& subdivision) {
    if (!free_payload) alloc_new_block();

    subdivision = {};
    subdivision.p = free_payload;

    free_payload = free_payload->free_next;
}

void Front::deinit(Subdivision& subdivision) {
    bool leaf = is_leaf(subdivision);

    if (leaf) {
        for (uint i = 0; i < subdivision.count; i++) {
            cell_handle cell = subdivision.p->cells[i];
            cell_info[cell.id] = {};
        }
    }
    else {
        for (uint i = 0; i < 8; i++) deinit(subdivision.p->children[i]);
    }

    subdivision.p->free_next = free_payload;
    free_payload = subdivision.p;
}

void Front::alloc_new_block() {
    //todo cleanup alloced blocks
    uint count = BLOCK_SIZE / sizeof(Payload);
    Payload* payloads = new Payload[count]; //TEMPORARY_ARRAY(Payload, count);

    for (uint i = 1; i < count; i++) {
        payloads[i-1].free_next = payloads + i;
    }
    payloads[count-1].free_next = nullptr;

    free_payload = payloads;
}

bool Front::intersects(Subdivision& subdivision, const Ray& ray, const AABB& aabb) {
    if (!aabb.intersects(subdivision.aabb)) return false;

    bool leaf = is_leaf(subdivision);
    if (leaf) {
        for (uint i = 0; i < subdivision.count; i++) {
            if (!subdivision.p->aabbs[i].intersects(aabb)) continue;

            CFDCell& cell = cells[subdivision.p->cells[i].id];
            const ShapeDesc& shape = shapes[cell.type];
            for (uint f = 0; f < shape.num_faces; f++) {
                vec3 p[4];
                get_positions(vertices, cell, shape.faces[f], p);
                
                float t;
                if (ray_triangle_intersect(ray, p, &t)) {
                    return true;
                }
                if (shape.faces[f].num_verts == 4) {
                    vec3 t2[] = {p[0], p[2], p[3]};
                    if (ray_triangle_intersect(ray, t2, &t)) {
                        return true;
                    }
                }
            }
        }
    }
    else {
        for (uint i = 0; i < 8; i++) {
            if (intersects(subdivision.p->children[i], ray, aabb)) return true;
        }
    }
    return false;
}

bool Front::intersects(const Ray& ray) {
    Subdivision* start_from = last_visited;

    vec3 a = ray.orig;
    vec3 b = ray.orig + ray.t * ray.dir;

    AABB aabb;
    aabb.update(a);
    aabb.update(b);
    
    while (!aabb.inside(start_from->aabb_division)) {
        if (!start_from->parent) break; //found root
        start_from = start_from->parent;
    }

    return intersects(root, ray, aabb);
}

Front::Result Front::find_closest(Subdivision& subdivision, vec3 base, AABB& aabb, cell_handle curr) {
    Result result;
    if (!aabb.intersects(subdivision.aabb)) return result;

    glm::vec3 center = aabb.centroid();

    bool leaf = is_leaf(subdivision);
    if (leaf) {
        last_visited = &subdivision;

        for (uint i = 0; i < subdivision.count; i++) {
            if (subdivision.p->cells[i].id == curr.id) continue;
            if (!subdivision.p->aabbs[i].intersects(aabb)) continue;

            cell_handle cell_handle = subdivision.p->cells[i];
            CFDCell& cell = cells[cell_handle.id];


            //todo could skip this, if center is further than some threschold
            const ShapeDesc& shape = shapes[cell.type];

            uint verts = shapes[cell.type].num_verts;
            for (uint i = 0; i < verts; i++) {
                vertex_handle v = cell.vertices[i];
                vec3 position = vertices[v.id].position;
                float dist = length(position - center);

                if (result.vertex.id == v.id) {
                    result.cells[result.cell_count++] = cell_handle;
                }
                else if (dist < result.dist) {
                    //if (intersects(Ray(base, position))) continue;

                    result.dist = dist;
                    result.cell_count = 1;
                    result.cells[0] = cell_handle;
                    result.vertex = v;
                }
            }
        }
    }
    else {
        for (uint i = 0; i < 8; i++) {
            Result child_result = find_closest(subdivision.p->children[i], base, aabb, curr);
            if (child_result.dist < result.dist) result = child_result;
        }
    }

    return result;
}

Front::Result Front::find_closest(glm::vec3 position, vec3 base, float radius, cell_handle curr) {
    uint depth = 0;

    AABB sphere_aabb;
    sphere_aabb.min = position - glm::vec3(radius);
    sphere_aabb.max = position + glm::vec3(radius);

    Subdivision* start_from = last_visited;

    while (!sphere_aabb.inside(start_from->aabb_division)) {
        if (!start_from->parent) break; //found root
        start_from = start_from->parent;
    }

    Result result = find_closest(*start_from, base, sphere_aabb, curr);
    if (result.dist < radius) {
        return result;
    }
    else {
        return {};
    }
}

uint Front::centroid_to_index(glm::vec3 centroid, glm::vec3 min, glm::vec3 half_size) {
    glm::vec3 vec = (centroid - min + FLT_EPSILON) / (half_size + 2 * FLT_EPSILON);
    assert(vec.x >= 0.0f && vec.x <= 2.0f);
    assert(vec.y >= 0.0f && vec.y <= 2.0f);
    return (int)floorf(vec.x) + 4 * (int)floorf(vec.y) + 2 * (int)floorf(vec.z);
}

void Front::add_cell_to_leaf(Subdivision& subdivision, cell_handle handle, const AABB& aabb) {
    uint index = subdivision.count++;
    subdivision.aabb.update_aabb(aabb);
    subdivision.p->aabbs[index] = aabb;
    subdivision.p->cells[index] = handle;
    
    assert(subdivision.aabb.min.x != NAN);

    cell_info[handle.id].index = index;
    cell_info[handle.id].subdivision = &subdivision;
}

void Front::add_cell_to_sub(Subdivision divisions[8], cell_handle cell, const AABB& aabb, glm::vec3 min, glm::vec3 half_size) {
    glm::vec3 centroid = aabb.centroid();
    uint index = centroid_to_index(centroid, min, half_size);

    add_cell_to_leaf(divisions[index], cell, aabb);
}

void Front::add_cell(cell_handle handle) {
    cell_info.resize(handle.id + 1);

    CFDCell& cell = cells[handle.id];

    AABB aabb = {};
    for (uint i = 0; i < shapes[cell.type].num_verts; i++) {
        vec3 position = vertices[cell.vertices[i].id].position;
        aabb.update(position);
    }
    vec3 centroid = aabb.centroid();
    
    Subdivision* subdivision = &root;

    uint depth = 0;
    while (true) {
        bool leaf = is_leaf(*subdivision);

        vec3 half_size = 0.5f * subdivision->aabb_division.size();
        vec3 min = subdivision->aabb_division.min;

        if (leaf) {
            if (subdivision->count < MAX_PER_CELL) {
                add_cell_to_leaf(*subdivision, handle, aabb);
                break;
            }
            else { //Subdivide the cell
                cell_handle cells[MAX_PER_CELL];
                AABB aabbs[MAX_PER_CELL];
                memcpy_t(cells, subdivision->p->cells, subdivision->count);
                memcpy_t(aabbs, subdivision->p->aabbs, subdivision->count);
                //printf("Subdividing %p (%i)\n", subdivision, subdivision->count);

                glm::vec3 mins[8] = {
                    //bottom
                    {glm::vec3(min.x,               min.y,               min.z)}, //left front
                    {glm::vec3(min.x + half_size.x, min.y,               min.z)}, //right front
                    {glm::vec3(min.x,               min.y,               min.z + half_size.z)}, //left back
                    {glm::vec3(min.x + half_size.x, min.y,               min.z + half_size.z)}, //right back
                    //top
                    {glm::vec3(min.x,               min.y + half_size.y, min.z)}, //left front
                    {glm::vec3(min.x + half_size.x, min.y + half_size.y, min.z)}, //right front
                    {glm::vec3(min.x,               min.y + half_size.y, min.z + half_size.z)}, //left back
                    {glm::vec3(min.x + half_size.x, min.y + half_size.y, min.z + half_size.z)}, //right back
                };

                for (uint i = 0; i < 8; i++) {
                    init(subdivision->p->children[i]);
                    subdivision->p->children[i].aabb_division = { mins[i], mins[i] + half_size };
                    subdivision->p->children[i].parent = subdivision;
                }

                for (uint i = 0; i < MAX_PER_CELL; i++) {
                    add_cell_to_sub(subdivision->p->children, cells[i], aabbs[i], min, half_size);
                }
            }
        }

        uint index = centroid_to_index(centroid, min, half_size);

        subdivision->count++;
        subdivision->aabb.update_aabb(aabb);

        Subdivision* prev = subdivision;

        subdivision = subdivision->p->children + index;
        depth++;
    }
}

void Front::remove_cell(cell_handle handle) {
    CellInfo& info = cell_info[handle.id];
    Subdivision* subdivision = info.subdivision;

    uint index = info.index;
    uint last = subdivision->count - 1;

    AABB aabb = subdivision->p->aabbs[index];

    subdivision->p->aabbs[index] = subdivision->p->aabbs[last];
    subdivision->p->cells[index] = subdivision->p->cells[last];

    bool update_aabb = true;
    bool started_from = true;
    while (subdivision) {
        subdivision->count--;

        const AABB& sub_aabb = subdivision->aabb;
        if (update_aabb) {
            update_aabb = glm::any(glm::equal(sub_aabb.min, aabb.min))
                || glm::any(glm::equal(sub_aabb.max, aabb.max));
        }
        if (update_aabb) {
            subdivision->aabb = AABB();
            if (started_from) {
                for (uint i = 0; i < MAX_PER_CELL; i++) {
                    subdivision->aabb.update_aabb(subdivision->p->aabbs[i]);
                }
            }
            else {
                for (uint i = 0; i < 8; i++) {
                    subdivision->aabb.update_aabb(subdivision->p->children[i].aabb);
                }
            }
        }

        if (is_leaf(*subdivision) && !started_from) {
            Subdivision children[8];
            memcpy_t(children, subdivision->p->children, 8);

            subdivision->count = 0;
            for (uint i = 0; i < 8; i++) {
                Subdivision& child = children[i];

                memcpy_t(subdivision->p->aabbs + subdivision->count, child.p->aabbs, child.count);
                memcpy_t(subdivision->p->cells + subdivision->count, child.p->cells, child.count);

                subdivision->count += child.count;
                deinit(child);
            }

            assert(subdivision->count <= MAX_PER_CELL);
        }

        subdivision = subdivision->parent;
        started_from = false;
    }
}
