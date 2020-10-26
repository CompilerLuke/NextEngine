#pragma once

#include "core/reflection.h"
#include "core/container/sstring.h"
#include "core/container/slice.h"
#include "core/container/handle_manager.h"
#include "engine/handle.h"
#include "graphics/rhi/buffer.h"
#include "graphics/culling/aabb.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

const uint MAX_MESH_LOD = 8;

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 tex_coord;
	glm::vec3 tangent;
	glm::vec3 bitangent;
};

using MeshFlags = uint;
const MeshFlags MESH_WITH_NO_UVS = 1 << 0;

struct Mesh {
	uint lod_count;
	VertexBuffer buffer[MAX_MESH_LOD];
	slice<Vertex> vertices[MAX_MESH_LOD];
	slice<uint> indices[MAX_MESH_LOD];
	AABB aabb;
	uint material_id;
	MeshFlags flags;
};

struct Model {
	slice<Mesh> meshes;
	array<MAX_MESH_LOD, float> lod_distance;
	slice<sstring> materials;
	AABB aabb;
};

COMP
struct ModelRenderer {
	bool visible = true;
	model_handle model_id;
};

struct VertexBuffer;
struct VertexStreaming;
struct Assets;

VertexBuffer get_vertex_buffer(model_handle model, uint index, uint lod = 0);
void load_assimp(Model* model, string_view real_path, const glm::mat4& apply_transform);