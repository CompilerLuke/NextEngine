#include "mesh/input_mesh.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "core/container/hash_map.h"

//found on https://cs.stackexchange.com/questions/37952/hash-function-floating-point-inputs-for-genetic-algorithm
u64 hash_func(glm::vec3 position) {
	int h = 1;
	for (int i = 0; i < 3; i++) {
		union {
			float as_float;
			int as_int;
		} value = { position[i] };

		h = 31 * h + value.as_int;
	}
	h ^= (h >> 20) ^ (h >> 12);
	return h ^ (h >> 7) ^ (h >> 4);
}

template<typename T>
using spatial_hash_map = hash_map_base<vec3, T>;

template<typename T>
spatial_hash_map<T> make_t_hash_map(uint hash_map_size) {
	return spatial_hash_map<T>(hash_map_size,
		TEMPORARY_ZEROED_ARRAY(hash_meta, hash_map_size),
		TEMPORARY_ZEROED_ARRAY(vec3, hash_map_size),
		TEMPORARY_ZEROED_ARRAY(T, hash_map_size)
		);
}

SurfaceTriMesh surface_from_mesh(const glm::mat4& mat, Mesh& mesh) {
	uint lod = 0;
	
	slice<uint> mesh_indices = mesh.indices[lod];
	slice<Vertex> mesh_vertices = mesh.vertices[lod];

	uint hash_map_size = mesh_indices.length * 3;

	spatial_hash_map<uint> vertex_hash_map = make_t_hash_map<uint>(hash_map_size);

	hash_map_base<EdgeSet, tri_handle> edge_hash_map;
	edge_hash_map.capacity = hash_map_size;
	edge_hash_map.meta = TEMPORARY_ZEROED_ARRAY(hash_meta, hash_map_size);
	edge_hash_map.keys = TEMPORARY_ZEROED_ARRAY(EdgeSet, hash_map_size);
	edge_hash_map.values = TEMPORARY_ZEROED_ARRAY(tri_handle, hash_map_size);

	uint vertex_id = 1;

	SurfaceTriMesh surface;
	surface.resize_tris(mesh_indices.length / 3 + 1);

	//leave index 0,3 of tets unoccpied, which is the null value
	//leave index 0 of positions unoccupied, which is the null value
	for (uint i = 0; i < mesh_indices.length; i += 3) {
		uint* indices = mesh_indices.data + i;
		vec3 vertex_positions[3] = {};

		for (uint j = 0; j < 3; j++) {
			Vertex vertex = mesh.vertices[lod][indices[j]];

			vec3 position = vertex.position;
			uint& id = vertex_hash_map[position]; //deduplicate vertices
			if (id == 0) { id = vertex_id++; } //assign the vertex an ID

			surface.indices[i + j + 3] = { (int)id };
		}

		for (uint j = 0; j < 3; j++) {
			EdgeSet edge_set = {
				surface.indices[i + j + 3],
				surface.indices[i + (j + 1) % 3 + 3]
			};

			edge_handle edge = i + j + 3;

			edge_handle& neighbor_edge = edge_hash_map[edge_set];
			if (!neighbor_edge) neighbor_edge = edge;
			else {
				surface.edges[edge] = neighbor_edge;
				surface.edges[neighbor_edge] = edge;
			}
		}
	}

	surface.stable_to_edge.resize(surface.tri_count * 3);
	surface.edge_flags = new char[surface.stable_to_edge.length]();

	//todo: robustness, consider deallocated triangles
	for (uint i = 0; i < surface.tri_count * 3; i++) {
		surface.stable_to_edge[i] = i;
		surface.edge_to_stable[i] = { i };
	}

	surface.positions.resize(vertex_id);

	for (auto [position, id] : vertex_hash_map) {
		surface.positions[id] = vec3(mat * glm::vec4(glm::vec3(position), 1.0));
	}

	return surface;
}

//todo eventually this should be able to load real cad files
//in the meantime we will copy the data from the current model loader
input_model_handle InputMeshRegistry::load_model(string_view path, const glm::mat4& mat) {
	model_handle model_handle = load_Model(path, true, mat);

	Model* model = get_Model(model_handle);

	InputModel input;

	u64 vertices = 0;
	u64 indices = 0;
	for (Mesh mesh : model->meshes) {
		vertices += mesh.vertices[0].length;
		indices += mesh.indices[0].length;
	}

	input.meshes.reserve(model->meshes.length);
	input.surface[0] = std::move(surface_from_mesh(glm::mat4(1.0), model->meshes[0]));
	input.surface[0].aabb = model->aabb;

	input.aabb = model->aabb;

	vertices = 0;
	indices = 0;
	for (Mesh mesh : model->meshes) {
		InputMesh input_mesh;
		input_mesh.vertex_buffer = mesh.buffer[0];
		input.meshes.append(input_mesh);
	}

	return register_model(std::move(input));
}