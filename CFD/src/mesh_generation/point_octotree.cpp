#include "mesh_generation/point_octotree.h"
#include "mesh_generation/cross_field.h"
#include <glm/gtc/matrix_transform.hpp>

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

//can be optimized with an early out if you only care about wether a point is smaller than a certain length
void PointOctotree::find_closest(Subdivision& subdivision, AABB& aabb, vertex_handle& closest_vert, const glm::mat4& tensor, float& min_dist) {
    if (!aabb.intersects(subdivision.aabb)) return;

    vec3 center = aabb.centroid();

    bool leaf = is_leaf(subdivision);
    if (leaf) {
        last_visited = &subdivision;

        Payload& p = *subdivision.p;

        for (uint i = 0; i < subdivision.count; i++) {
            vertex_handle vert = p.vertices[i];
            glm::vec4 in_tensor_space = tensor * glm::vec4(glm::vec3(positions[vert.id]), 1.0);
            
            vec3 relative = positions[vert.id] - center;
            float dist2 = length(relative);
            float dist3 = length(vec3(in_tensor_space));
            
            //L-infinity norm
            float dist = fmaxf(fmaxf(fabs(in_tensor_space.x), fabs(in_tensor_space.y)), fabs(in_tensor_space.z));
            //L-2 norm: 
            //float dist = length(positions[vert.id] - center);

            if (dist < min_dist) {
                closest_vert = vert;
                min_dist = dist;
            }
        }
    }
    else {
        for (uint i = 0; i < 8; i++) {
            find_closest(subdivision.p->children[i], aabb, closest_vert, tensor, min_dist);
        }
    }
}

vertex_handle PointOctotree::find_closest(vec3 position, const Cross& cross, float radius) {
    uint depth = 0;
    
    glm::mat4 translation = glm::translate(glm::mat4(1.0), glm::vec3(position));
    glm::mat4 rotation = cross.rotation_mat();
    glm::mat4 tensor;
    if (cross.tangent == 0) {
        tensor = translation;
    }
    else tensor = translation * rotation;

    AABB aabb;
    aabb.min = -glm::vec3(radius);
    aabb.max = glm::vec3(radius);
    aabb = aabb.apply(tensor);
    
    vec3 center = aabb.centroid();
    
    glm::mat4 inv_tensor = glm::inverse(tensor);

    Subdivision* start_from = last_visited;

    while (!aabb.inside(start_from->aabb)) {
        if (!start_from->parent) break; //found root
        start_from = start_from->parent;
    }

    vertex_handle result;
    float closest_distance = radius;

    find_closest(*start_from, aabb, result, inv_tensor, closest_distance);

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
                
                AABB child_aabbs[8];
                subdivide_aabb(subdivision->aabb, child_aabbs);

                for (uint i = 0; i < 8; i++) {
                    init(subdivision->p->children[i]);
                    subdivision->p->children[i].aabb = child_aabbs[i];
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
