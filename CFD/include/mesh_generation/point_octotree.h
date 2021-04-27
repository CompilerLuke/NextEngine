#pragma once

#include "cfd_core.h"
#include "core/math/aabb.h"
#include "mesh.h"
#include "core/container/vector.h"

struct Ray;

struct PointOctotree {
    static constexpr uint MAX_PER_CELL = 16;
    static constexpr uint BLOCK_SIZE = kb(16);

    union Payload;

    struct Subdivision {
        AABB aabb;
        uint count;
        Payload* p; //to avoid recursion
        Subdivision* parent;
    };

    union Payload {
        Subdivision children[8];
        struct {
            vertex_handle vertices[MAX_PER_CELL];
        };
        Payload* free_next;

        Payload() {
            memset(this, 0, sizeof(Payload));
        }
    };

    struct CellInfo {
        Subdivision* subdivision;
        uint index;
    };

    Payload* free_payload;
    Subdivision root;
    AABB grid_bounds;
    vector<vec3>& positions;

    Subdivision* last_visited;


    PointOctotree(vector<vec3>& positions, const AABB& aabb);
    bool is_leaf(Subdivision& subdivision);
    vertex_handle find_closest(vec3 position, float radius);
    uint centroid_to_index(glm::vec3 centroid, glm::vec3 min, glm::vec3 half_size);
    void add_vert(vertex_handle vert);

private:
    void init(Subdivision& subdivision);
    void deinit(Subdivision& subdivision);
    void alloc_new_block();
    void find_closest(Subdivision& subdivision, AABB& aabb, vertex_handle& closest_vert, float& min_dist);
    void add_vert_to_leaf(Subdivision& subdivision, vertex_handle vert);
};
