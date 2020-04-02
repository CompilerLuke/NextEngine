#include "stdafx.h"
#include "graphics/assets/asset_manager.h"
#include "core/container/hash_map.h"
#include "core/container/sstring.h"
#include "core/serializer.h"

AssetManager::AssetManager(Level& level) : level(level), models(level), shaders(level), textures(level) {}

//REFLECT_STRUCT_BEGIN(Model)
//REFLECT_STRUCT_MEMBER(path)
//REFLECT_STRUCT_MEMBER(meshes)
//REFLECT_STRUCT_MEMBER(materials)
//REFLECT_STRUCT_END()

/*
#define MAX_MODELS 100
#define MAX_TEXTURES 100

struct AssetCache {
	hash_map<asset_path, Model, MAX_MODELS> models_path;
	hash_map<asset_path, Texture, MAX_TEXTURES> textures_path;
};

AssetCache cache;

Model& get_model(model_handle handle) {
	assert(cache.models_path.keys.is_full(handle.id));
	return cache.models_path.values[handle.id];
}

constexpr int CHUNK_SIZE = 1024 * 1024;

void load_asset_chunk(DeserializerBuffer& chunk) {
	int model_count = chunk.read_int();

	int vertices_length = chunk.read_int();
	int indices_length = chunk.read_int();

	Vertex* vertices = (Vertex*)chunk.pointer_to_n_bytes(sizeof(Vertex) * vertices_length);
	uint* indices = (uint*)chunk.pointer_to_n_bytes(sizeof(uint) * indices_length);

	VertexBuffer vertex_buffer = RHI::alloc_vertex_buffer(VERTEX_LAYOUT_DEFAULT, vertices_length, vertices, indices_length, indices);

	for (int i = 0; i < model_count; i++) {
		uint handle = chunk.read_int();

		Model& model = cache.models_path.values[handle];
		model.path = *(asset_path*)chunk.pointer_to_n_bytes(sizeof(asset_path));
		model.meshes.length = chunk.read_int();
		model.meshes.data = (Mesh*)chunk.pointer_to_n_bytes(sizeof(Mesh) * model.meshes.length);
		model.vertices.length = chunk.read_int();
		model.vertices.data = (Vertex*)((char*)chunk.data + chunk.read_int());
		model.indices.length = chunk.read_int();
		model.indices.data = (uint*)((char*)chunk.data + chunk.read_int());
		model.aabb = *(AABB*)chunk.pointer_to_n_bytes(sizeof(AABB));

		for (Mesh& mesh : model.meshes) {
			mesh.buffer = vertex_buffer;
			mesh.vertices_offset += mesh.vertices_offset;
			mesh.indices_offset += mesh.indices_offset;
		}
	}
}

model_handle load_Model(string_view path, bool serialized) { //TODO ADD MORE SUPPORT IN HASH_MAP FOR WHAT WE ARE DOING
	int index = cache.models_path.keys.index(path.c_str());
	if (index != -1) return { index };

	//todo add support for string_view

	cache.models_path.keys.add(path.c_str());

	load({ index }, glm::mat4(1.0));

	return { index };
}

*/