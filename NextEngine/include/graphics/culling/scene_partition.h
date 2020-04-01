#pragma once

#include "aabb.h"

#define MAX_NODES 500
#define MAX_MESH_INSTANCES 10000

struct Node {
	AABB aabb;
	int offset;
	int count;
	Node* child[2];
};

struct ScenePartition {
	int meshes_count = 0;
	AABB aabbs[MAX_MESH_INSTANCES];
	int meshes[MAX_MESH_INSTANCES];
	glm::mat4 model_m[MAX_MESH_INSTANCES];

	int node_count = 0;
	Node nodes[MAX_NODES];
};