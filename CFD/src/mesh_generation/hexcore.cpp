#include "mesh_generation/hexcore.h"
#include "core/math/aabb.h"
#include "core/container/tvector.h"
#include "mesh_generation/point_octotree.h"

#include <set>

struct HexcoreCell {
    u64 morton;
    uint depth; //todo could compute this implicitly, but probably more error prone
    bool front;
    union {
        HexcoreCell* parent;
        HexcoreCell* next_free;
    };
    HexcoreCell* children;
};

const uint CHUNK_SIZE = mb(50);

HexcoreCell* alloc_8_hexcore_cell(HexcoreCell** pool) {
    if (!*pool) {
        *pool = (HexcoreCell*)calloc(1,CHUNK_SIZE);
        uint n = CHUNK_SIZE / (8 * sizeof(HexcoreCell));
        for (uint i = 0; i < n - 1; i++) {
            (*pool)[i * 8].next_free = *pool + (i + 1) * 8;
        }
        (*pool)[(n - 1) * 8].next_free = nullptr;
    }

    HexcoreCell* current = *pool;
    *pool = current->next_free;
    return current;
}

void hexcore_to_mesh(CFDVolume& volume, HexcoreCell* root, const AABB& aabb) {
    struct Data {
        AABB aabb;
        HexcoreCell* cells;
    };

    tvector<Data> stack;
    stack.append({ aabb, root->children });

    while (stack.length > 0) {
        Data data = stack.pop();

        AABB child_aabbs[8];
        subdivide_aabb(data.aabb, child_aabbs);

        for (uint i = 0; i < 8; i++) {
            if (!data.cells[i].children) {
                if (!data.cells[i].front) continue;
                uint subdivision = 1;
                vec3 dx = child_aabbs[i].size() / subdivision;

                for (uint x = 0; x < subdivision; x++) {
                    for (uint y = 0; y < subdivision; y++) {
                        for (uint z = 0; z < subdivision; z++) {
                            AABB aabb;
                            aabb.min = child_aabbs[i].min + vec3(x, y, z) * dx;
                            aabb.max = aabb.min + dx;

                            glm::vec3 points[8];
                            aabb.to_verts(points);

                            CFDCell cell{ CFDCell::HEXAHEDRON };

                            for (uint j = 0; j < 8; j++) {
                                cell.vertices[j] = { (int)volume.vertices.length };
                                volume.vertices.append({ points[j] });
                            }

                            volume.cells.append(cell);
                        }
                    }
                }


            }
            else {
                stack.append({ child_aabbs[i], data.cells[i].children });
            }
        }
    }
}

#define MORTON_MASK(depth, i) ((u64)(i) << (depth-1)*3)
#define MORTON_AXIS(depth, k) ((u64)(1) << ((depth-1)*3+k))

// Assumes little endian
void printBits(size_t const size, void const* const ptr)
{
    unsigned char* b = (unsigned char*)ptr;
    unsigned char byte;
    int i, j;

    for (i = size - 1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}

void build_hexcore(HexcoreCell** pool, HexcoreCell* root, PointOctotree& octo) {
    struct Data {
        HexcoreCell* parent;
        HexcoreCell* cells;
        PointOctotree::Payload* payload;
    };

    root->children = alloc_8_hexcore_cell(pool);

    tvector<Data> stack;
    stack.append({ root, root->children, octo.root.p });

    while (stack.length > 0) {
        Data data = stack.pop();
        HexcoreCell* cells = data.cells;
        u64 morton = data.parent->morton;
        uint depth = data.parent->depth + 1;

        for (uint i = 0; i < 8; i++) {
            auto& subdivision = data.payload->children[i];
            u64 child_morton = morton | MORTON_MASK(depth, i);
            cells[i].morton = child_morton;
            cells[i].parent = data.parent;
            cells[i].depth = depth;

            if (subdivision.count <= PointOctotree::MAX_PER_CELL) {
                cells[i].children = nullptr;
                cells[i].front = subdivision.count > 0;
            }
            else {
                cells[i].children = alloc_8_hexcore_cell(pool);
                stack.append({ cells + i, cells[i].children, subdivision.p });
            }
        }
    }
}

struct HexRefinementQueue {
    vector<HexcoreCell*> refine;
    std::set<u64> morton_codes;
};

u64 neighbor_code(u64 morton, uint depth, uint k, uint* f) {
    u64 mask = MORTON_AXIS(depth, k);
    bool b = morton & mask;
    u64 neighbor = morton ^ mask;
    for (uint i = depth - 1; i > 0; i--) {
        u64 mask = MORTON_AXIS(i, k);
        neighbor ^= mask;
        if (b != bool(morton & mask)) {
            *f = i;
            return neighbor;
        }
    }

    return UINT64_MAX;
}

HexcoreCell* find_cell(HexcoreCell* start, u64 neighbor, uint max_depth, uint f) {
    if (neighbor == UINT64_MAX) return nullptr;

    HexcoreCell* current = start;
    while (current && current->depth >= f) { //(current->morton & morton) != current->morton ) {
        current = current->parent;
    }
    if (!current) return nullptr;

    while (current->children && current->depth < max_depth) {
        uint index = (neighbor >> 3 * current->depth) & (1 << 3) - 1;
        current = current->children + index;
    }

    if (current->children) return nullptr;
    return current;
}

void refine_children_if_needed(HexRefinementQueue& queue, HexcoreCell* children, uint n) {
    for (uint i = 0; i < n; i++) {
        HexcoreCell& cell = children[i];
        if (!cell.front) continue;

        uint depth = cell.depth;
        u64 morton = cell.morton;

        for (uint k = 0; k < 3; k++) {
            uint f;
            u64 neighbor = neighbor_code(morton, depth, k, &f);
            HexcoreCell* neighbor_cell = find_cell(cell.parent, neighbor, depth, f);
            if (!neighbor_cell) continue;
            if (neighbor_cell->front) continue;

            if (queue.morton_codes.find(neighbor_cell->morton) != queue.morton_codes.end()) continue;
            //neighbor_cell->depth >= depth - 1 ||

            queue.refine.append(neighbor_cell);
            queue.morton_codes.insert(neighbor_cell->morton);
        }
    }
}

void balance_hexcore(HexcoreCell* root, HexcoreCell** pool) {
    struct Data {
        HexcoreCell* cells;
    };

    HexRefinementQueue queue;

    tvector<Data> stack;

    uint target_subdivision = 4;

    uint count = 0;
    while (count++ < 0) {
        stack.append({ root->children });

        while (stack.length > 0) {
            Data data = stack.pop();

            for (uint i = 0; i < 8; i++) {
                HexcoreCell& cell = data.cells[i];

                if (cell.children) {
                    stack.append({ cell.children });
                    continue;
                }

                refine_children_if_needed(queue, &cell, 1);
            }
        }

        if (queue.refine.length == 0) break;

        while (queue.refine.length > 0) {
            HexcoreCell* cell = queue.refine.pop();
            
            cell->children = alloc_8_hexcore_cell(pool);

            uint depth = cell->depth + 1;
            for (uint i = 0; i < 8; i++) {
                cell->children[i].parent = cell;
                cell->children[i].depth = depth;
                cell->children[i].morton = cell->morton | MORTON_MASK(depth, i);
                cell->children[i].children = 0;
                cell->children[i].front = true;

                if (depth < target_subdivision) {
                    //queue.refine.append(cell->children + i);
                }
            }
        }

        queue.refine.clear();
        queue.morton_codes.clear();
    }
}

void hexcore(PointOctotree& octo, CFDVolume& volume, CFDDebugRenderer& debug) {
    HexcoreCell* pool = nullptr;
    HexcoreCell root = {};
    build_hexcore(&pool, &root, octo);
    balance_hexcore(&root, &pool);
    hexcore_to_mesh(volume, &root, octo.root.aabb);
}