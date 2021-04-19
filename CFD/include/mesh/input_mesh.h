#pragma once

#include "cfd_components.h"
#include "core/container/sstring.h"
#include "core/container/vector.h"
#include "graphics/assets/model.h"
#include "surface_tet_mesh.h"

struct input_model_handle;

struct FeatureEdge {
	float sizing;
};

struct InputMesh {
	VertexBuffer vertex_buffer;
	/*todo slice into surface*/
};

struct InputMeshNode {
	sstring name;
	vector<uint> children;
	uint mesh;
};

struct InputModel {
	sstring name;
	AABB aabb;
	SurfaceTriMesh surface[MAX_MESH_LOD];
	vector<InputMesh> meshes;
	vector<edge_handle> feature_edges;
	InputMeshNode root;
};

struct InputModelBVH;
struct CFDDebugRenderer;

class InputMeshRegistry {
	CFDDebugRenderer& debug;
	vector<InputModel> models;
	vector<InputModelBVH> bvh;

public:
	InputMeshRegistry(CFDDebugRenderer&);
	~InputMeshRegistry();
	
	input_model_handle register_model(InputModel&&);
	input_model_handle load_model(string_view path, const glm::mat4& mat = glm::mat4(1.0));
	InputModel& get_model(input_model_handle);
	InputModelBVH& get_model_bvh(input_model_handle);
};