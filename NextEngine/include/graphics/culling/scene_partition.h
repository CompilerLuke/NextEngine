#pragma once

#include "core/core.h"
#include "aabb.h"

#define MAX_NODES 500
#define MAX_MESH_INSTANCES 10000

struct Node {
	AABB aabb;
	int offset;
	int count;
	Node* child[2];
};

struct Partition {
	int count = 0;
	int node_count = 0;
	Node nodes[MAX_NODES];
};

struct ScenePartition : Partition {
	AABB aabbs[MAX_MESH_INSTANCES];
	int meshes[MAX_MESH_INSTANCES];
	glm::mat4 model_m[MAX_MESH_INSTANCES];
};

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

ENGINE_API Node& alloc_leaf_node(Partition& scene_partition, AABB& aabb, int max, int count);
ENGINE_API Node& leaf_node(Node& node, AABB& aabb, int max, int count);
ENGINE_API Node& alloc_node(Partition& scene_partition);
BranchNodeInfo ENGINE_API alloc_branch_node(Partition& scene_partition, AABB& node_aabb);
int ENGINE_API elem_offset(Partition& scene_partition, BranchNodeInfo& info, AABB& aabb);
bool ENGINE_API bigger_than_leaf(BranchNodeInfo& info, AABB& aabb);
int ENGINE_API split_index(BranchNodeInfo& info, AABB& aabb);
void ENGINE_API bump_allocator(Partition& partition, BranchNodeInfo& info);