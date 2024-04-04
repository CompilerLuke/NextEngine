#pragma once

#include "scene_partition.h"
#include "core/memory/linear_allocator.h"

struct BranchNodeInfo {
	Node& node;
	AABB child_aabbs[2];
	int axis;
	glm::vec3 pivot;
	float half_size;
};

template<typename T>
void copy_into_node(Node& node, T* to, T* from) {
	memcpy(to + node.offset, from, sizeof(T) * node.count);
}

inline Node& alloc_node(Partition& scene_partition) {
	assert(scene_partition.count <= MAX_MESH_INSTANCES);

	uint offset = ++scene_partition.node_count - 1;
	Node& node = scene_partition.nodes[offset];
	node = {};

	return node;
}

inline Node& alloc_leaf_node(Partition& scene_partition, AABB& node_aabb, int max, int count) {
	Node& node = alloc_node(scene_partition);
	node.aabb = node_aabb;
	
	uint offset = (scene_partition.count += count) - count;
	node.offset = offset;
	node.count = count;

	assert(scene_partition.count <= max);

	return node;
}

//todo pass in allocator
inline BranchNodeInfo alloc_branch_node(Partition& scene_partition, AABB& node_aabb) {
	BranchNodeInfo info = { alloc_node(scene_partition) };
	glm::vec3 size = node_aabb.size();

	info.axis = size.x > size.y ? (size.x > size.z ? 0 : 2) : (size.y > size.z ? 1 : 2);
	info.pivot = node_aabb.centroid();
	info.half_size = size[info.axis] * 0.5f;

	return info;
}

inline uint split_aabbs(Partition& scene_partition, BranchNodeInfo& info, slice<AABB> aabbs, int* result) {
    uint count_bigger_than_leaf = 0;
    for (int i = 0; i < aabbs.length; i++) {
        AABB& aabb = aabbs[i];
        
        if (aabb.size()[info.axis] > info.half_size) {
            result[i] = -1;
            count_bigger_than_leaf++;
        } else {
            int node_index = aabb.centroid()[info.axis] > info.pivot[info.axis];
            result[i] = node_index;
            info.child_aabbs[node_index].update_aabb(aabb);
        }
    }
    
    info.node.offset = (scene_partition.count += count_bigger_than_leaf) - count_bigger_than_leaf;
    info.node.count = count_bigger_than_leaf;
    
    return info.node.offset;
}
