#pragma once

#include "scene_partition.h"
#include "core/memory/linear_allocator.h"

struct BranchNodeInfo {
	Node& node;
	AABB child_aabbs[2];
	int watermark;
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

	Node& node = scene_partition.nodes[scene_partition.node_count++];
	node = {};
	node.offset = scene_partition.count;

	return node;
}

inline Node& alloc_leaf_node(Partition& scene_partition, AABB& node_aabb, int max, int count) {
	Node& node = alloc_node(scene_partition);
	node.aabb = node_aabb;
	node.count = count;

	scene_partition.count += count;

	assert(scene_partition.count <= max);

	return node;
}

inline BranchNodeInfo alloc_branch_node(Partition& scene_partition, AABB& node_aabb) {
	BranchNodeInfo info = { alloc_node(scene_partition) };
	glm::vec3 size = node_aabb.size();

	info.watermark = temporary_allocator.occupied;
	info.axis = size.x > size.y ? (size.x > size.z ? 0 : 2) : (size.y > size.z ? 1 : 2);
	info.pivot = 0.5f * node_aabb.centroid();
	info.half_size = size[info.axis] * 0.5f;

	return info;
}

inline int elem_offset(Partition& scene_partition, BranchNodeInfo& info, AABB& aabb) {
	int offset = scene_partition.count++;
	assert(scene_partition.count <= MAX_MESH_INSTANCES);
	info.node.aabb.update_aabb(aabb);
	info.node.count++;

	return offset;
}

inline bool bigger_than_leaf(BranchNodeInfo& info, AABB& aabb) {
	return aabb.size()[info.axis] > info.half_size;
}

inline int split_index(BranchNodeInfo& info, AABB& aabb) {
	int node_index = aabb.centroid()[info.axis] > info.pivot[info.axis];
	info.child_aabbs[node_index].update_aabb(aabb);
	return node_index;
}

inline void bump_allocator(Partition& partition, BranchNodeInfo& info) {
	temporary_allocator.occupied = info.watermark;
	partition.count += info.node.count;
}