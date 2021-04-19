#include "core/math/aabb.h"
#include "mesh.h"
#include "core/container/vector.h"

struct Ray;

struct Front {
    static constexpr uint MAX_PER_CELL = 16;
    static constexpr uint BLOCK_SIZE = kb(16);

    union Payload;
    
    struct Subdivision {
        AABB aabb; //adjusts to content
        AABB aabb_division; //always exactly half of parent
        uint count;
        Payload* p; //to avoid recursion
        Subdivision* parent;
    };
    
    union Payload {
        Subdivision children[8];
        struct {
            AABB aabbs[MAX_PER_CELL];
            cell_handle cells[MAX_PER_CELL];
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

    struct Result {
        uint cell_count;
        cell_handle cells[16];
        vertex_handle vertex;
        float dist = FLT_MAX;
        float dist_to_center = FLT_MAX;
    };

    Payload* free_payload;
    Subdivision root;
    AABB grid_bounds;
    vector<CFDVertex>& vertices;
    vector<CFDCell>& cells;
    vector<CellInfo> cell_info;

    Subdivision* last_visited;

    
    Front(vector<CFDVertex>& vertices, vector<CFDCell>& cells, const AABB& aabb);
    bool is_leaf(Subdivision& subdivision);
    bool intersects(const Ray& ray);
    Result find_closest(glm::vec3 position, vec3 base, float radius, cell_handle curr);
    uint centroid_to_index(glm::vec3 centroid, glm::vec3 min, glm::vec3 half_size);
    void add_cell(cell_handle handle);
    
private:
    void init(Subdivision& subdivision);
    void deinit(Subdivision& subdivision);
    void alloc_new_block();
    bool intersects(Subdivision& subdivision, const Ray& ray, const AABB& aabb);
    Result find_closest(Subdivision& subdivision, vec3 base, AABB& aabb, cell_handle curr);
    void remove_cell(cell_handle handle);
    void add_cell_to_leaf(Subdivision& subdivision, cell_handle handle, const AABB& aabb);
    void add_cell_to_sub(Subdivision divisions[8], cell_handle cell, const AABB& aabb, glm::vec3 min, glm::vec3 half_size);
    void add_cell(Subdivision& subdivision, cell_handle handle, glm::vec3 centroid, const AABB& aabb);
};
