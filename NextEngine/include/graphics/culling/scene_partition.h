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