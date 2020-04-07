#include "stdafx.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <assimp/vector2.h>
#include <assimp/postprocess.h>
#include "core/io/vfs.h"
#include "core/io/logger.h"
#include "graphics/assets/model.h"
#include "core/memory/linear_allocator.h"

struct ModelLoadingScratch {
	uint mesh_count;
	uint vertices_count;
	uint indices_count;
	
	const aiScene* scene;
	glm::mat3 apply_trans;
	Mesh* meshes_base;
	Vertex* vertex_base;
	uint* indices_base;
};


 void process_Mesh(ModelLoadingScratch* scratch, aiMesh* mesh) {
	glm::mat3 apply_trans3 = scratch->apply_trans;
	 
	//SUB ALLOCATE VERTICES
	uint vertices_count = mesh->mNumVertices;
	auto vertices = scratch->vertex_base + scratch->vertices_count;
	scratch->vertices_count += vertices_count;

	//FILL VERTICES
	AABB aabb;

	for (int i = 0; i < mesh->mNumVertices; i++) {
		auto position = mesh->mVertices[i];

		auto tangent = mesh->mTangents[i];
		auto bitangent = mesh->mBitangents[i];

		auto coords = mesh->mTextureCoords[0];
		if (!coords) throw "Mesh does't have uvs";

		auto first_coords = coords[i];
		auto normals = mesh->mNormals[i];

		vertices[i] = {
			glm::vec3(position.x, position.y, position.z) * apply_trans3,
			glm::vec3(normals.x, normals.y, normals.z) * apply_trans3,
			glm::vec2(first_coords.x, first_coords.y),
			glm::vec3(tangent.x, tangent.y, tangent.z) * apply_trans3,
			glm::vec3(bitangent.x, bitangent.y, bitangent.z) * apply_trans3
		};

		aabb.update(vertices[i].position);
	}

	//FILL INDICES

	uint* indices = scratch->indices_base + scratch->indices_count;
	int indices_count = 0;

	//TODO could assume them to always be three
	for (int i = 0; i < mesh->mNumFaces; i++) {
		auto face = mesh->mFaces[i];
		memcpy(indices + indices_count, face.mIndices, sizeof(unsigned int) * face.mNumIndices);
		indices_count += face.mNumIndices;
	}

	scratch->indices_count += indices_count;

	Mesh* new_mesh = scratch->meshes_base + scratch->mesh_count;
	new_mesh->vertices.length = vertices_count;
	new_mesh->indices.length = indices_count;
	new_mesh->vertices.data = vertices;
	new_mesh->indices.data = indices;
	new_mesh->aabb = aabb;
	new_mesh->material_id = mesh->mMaterialIndex;

	scratch->mesh_count++;
}

void process_size(aiNode* node, const aiScene* scene, ModelLoadingScratch* scratch) {
	for (int i = 0; i < node->mNumMeshes; i++) {
		auto mesh_id = node->mMeshes[i];
		auto mesh = scene->mMeshes[mesh_id];
		
		scratch->vertices_count += mesh->mNumVertices;
		for (int i = 0; i < mesh->mNumFaces; i++) scratch->indices_count += mesh->mFaces[i].mNumIndices;
	}

	for (int i = 0; i < node->mNumChildren; i++) {
		process_size(node->mChildren[i], scene, scratch);
	}
}

void process_Node(ModelLoadingScratch* scratch, aiNode* node) {
	for (int i = 0; i < node->mNumMeshes; i++) {
		auto mesh_id = node->mMeshes[i];
		auto mesh = scratch->scene->mMeshes[mesh_id];

		process_Mesh(scratch, mesh);
	}

	for (int i = 0; i < node->mNumChildren; i++) {
		process_Node(scratch, node->mChildren[i]);
	}
}

model_handle ModelManager::load(string_view path, bool serialized) {
	for (int i = 0; i < slots.length; i++) { //todo use hashmap for optimization
		auto& slot = slots[i];
		if (slot.path == path) {
			return index_to_handle(i);
		}
	}

	model_handle handle = assign_handle({ path }, serialized);
	load_in_place(handle, glm::mat4(1.0));
	return handle;
}

void ModelManager::load_in_place(model_handle handle, const glm::mat4& apply_transform) { //TODO WHEN REIMPORTED, can reuse same memory
	Model* model = get(handle);

	auto real_path = level.asset_path(model->path);

	Assimp::Importer importer;
	auto scene = importer.ReadFile(real_path.c_str(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
	if (!scene) throw string_buffer("Could not load model ") + model->path + " " + real_path;

	log("Loading model ", model->path, "\n");


	ModelLoadingScratch scratch = {};
	scratch.mesh_count = scene->mNumMeshes;
	
	process_size(scene->mRootNode, scene, &scratch);

	scratch.meshes_base  = PERMANENT_ARRAY(Mesh,   scratch.mesh_count);
	scratch.vertex_base  = PERMANENT_ARRAY(Vertex, scratch.vertices_count);
	scratch.indices_base = PERMANENT_ARRAY(uint,   scratch.indices_count);


	printf("\nLoading model %s\n", model->path);
	printf("\tMeshes: %i\n", scratch.mesh_count);
	printf("\tVertices: %i\n", scratch.vertices_count);
	printf("\tIndices: %i\n", scratch.indices_count);

	scratch.mesh_count = 0;
	scratch.vertices_count = 0;
	scratch.indices_count = 0;
	scratch.scene = scene;
	scratch.apply_trans = apply_transform;

	process_Node(&scratch, scene->mRootNode);

	model->meshes.length = scratch.mesh_count;
	model->meshes.data = scratch.meshes_base;
	
	//todo merge into one upload
	for (int i = 0; i < scratch.mesh_count; i++) {
		Mesh* mesh = scratch.meshes_base + i;

		model->aabb.update_aabb(mesh->aabb);
		mesh->buffer = alloc_vertex_buffer(buffer_manager, VERTEX_LAYOUT_DEFAULT, mesh->vertices, mesh->indices);
	}

	model->materials.length = scene->mNumMaterials;
	model->materials.data = PERMANENT_ARRAY(sstring, model->materials.length);
	
	for (int i = 0; i < model->materials.length; i++) {
		auto aMat = scene->mMaterials[i];
		aiString c_name;
		aiGetMaterialString(aMat, AI_MATKEY_NAME, &c_name);

		printf("Material name %s\n", c_name.data);

		model->materials[i] = c_name.data;
	}
}