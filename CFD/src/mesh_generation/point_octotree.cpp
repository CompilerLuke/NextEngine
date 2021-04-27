#include "mesh_generation/point_octotree.h"

PointOctotree::PointOctotree(vector<vec3>& positions, const AABB& aabb)
    : positions(positions) {
    root = {};
    free_payload = nullptr;
    init(root);
    root.aabb = aabb;
    last_visited = &root; //note: self-referential so moving this struct will cause problems!
}

bool PointOctotree::is_leaf(Subdivision& subdivision) {
    return subdivision.count <= MAX_PER_CELL;
}

void PointOctotree::init(Subdivision& subdivision) {
    if (!free_payload) alloc_new_block();

    subdivision = {};
    subdivision.p = free_payload;

    free_payload = free_payload->free_next;
}

void PointOctotree::deinit(Subdivision& subdivision) {
    bool leaf = is_leaf(subdivision);

    if (leaf) {
        for (uint i = 0; i < subdivision.count; i++) {
            //no cleanup necessary
        }
    }
    else {
        for (uint i = 0; i < 8; i++) deinit(subdivision.p->children[i]);
    }

    subdivision.p->free_next = free_payload;
    free_payload = subdivision.p;
}

void PointOctotree::alloc_new_block() {
    //todo cleanup alloced blocks
    uint count = BLOCK_SIZE / sizeof(Payload);
    Payload* payloads = new Payload[count]; //TEMPORARY_ARRAY(Payload, count);

    for (uint i = 1; i < count; i++) {
        payloads[i - 1].free_next = payloads + i;
    }
    payloads[count - 1].free_next = nullptr;

    free_payload = payloads;
}

//todo use L-infinity norm
void PointOctotree::find_closest(Subdivision& subdivision, AABB& aabb, vertex_handle& closest_vert, float& min_dist) {
    if (!aabb.intersects(subdivision.aabb)) return;

    vec3 center = aabb.centroid();

    bool leaf = is_leaf(subdivision);
    if (leaf) {
        last_visited = &subdivision;

        Payload& p = *subdivision.p;

        for (uint i = 0; i < subdivision.count; i++) {
            vertex_handle vert = p.vertices[i];
            vec3 position = positions[vert.id];
            float dist = length(position - center);

            if (dist < min_dist) {
                closest_vert = vert;
                min_dist = dist;
            }
        }
    }
    else {
        for (uint i = 0; i < 8; i++) {
            find_closest(subdivision.p->children[i], aabb, closest_vert, min_dist);
        }
    }
}

vertex_handle PointOctotree::find_closest(vec3 position, float radius) {
    uint depth = 0;

    AABB sphere_aabb;
    sphere_aabb.min = position - glm::vec3(radius);
    sphere_aabb.max = position + glm::vec3(radius);

    Subdivision* start_from = last_visited;

    while (!sphere_aabb.inside(start_from->aabb)) {
        if (!start_from->parent) break; //found root
        start_from = start_from->parent;
    }

    vertex_handle result;
    float closest_distance = radius;

    find_closest(*start_from, sphere_aabb, result, closest_distance);

    return result;
}

uint PointOctotree::centroid_to_index(glm::vec3 centroid, glm::vec3 min, glm::vec3 half_size) {
    vec3 vec = (centroid - min + FLT_EPSILON) / (half_size + 2 * FLT_EPSILON);
    assert(vec.x >= 0.0f && vec.x <= 2.0f);
    assert(vec.y >= 0.0f && vec.y <= 2.0f);
    return (int)floorf(vec.x) + 4 * (int)floorf(vec.y) + 2 * (int)floorf(vec.z);
}

void PointOctotree::add_vert_to_leaf(Subdivision& subdivision, vertex_handle vert) {
    uint index = subdivision.count++;
    subdivision.p->vertices[index] = vert;
}

void PointOctotree::add_vert(vertex_handle vert) {
    vec3 position = positions[vert.id];

    Subdivision* subdivision = &root;

    uint depth = 0;
    while (true) {
        bool leaf = is_leaf(*subdivision);

        vec3 half_size = 0.5f * subdivision->aabb.size();
        vec3 min = subdivision->aabb.min;

        if (leaf) {
            if (subdivision->count < MAX_PER_CELL) {
                add_vert_to_leaf(*subdivision, vert);
                break;
            }
            else { //Subdivide the cell
                vertex_handle vertices[MAX_PER_CELL];
                memcpy_t(vertices, subdivision->p->vertices, subdivision->count);

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
                    subdivision->p->children[i].aabb = { mins[i], mins[i] + half_size };
                    subdivision->p->children[i].parent = subdivision;
                }

                for (uint i = 0; i < MAX_PER_CELL; i++) {
                    vertex_handle vert = vertices[i];

                    vec3 position = positions[vert.id];
                    uint index = centroid_to_index(position, min, half_size);

                    add_vert_to_leaf(subdivision->p->children[index], vert);
                }
            }
        }

        uint index = centroid_to_index(position, min, half_size);

        subdivision->count++;
        subdivision = subdivision->p->children + index;
        depth++;
    }
}