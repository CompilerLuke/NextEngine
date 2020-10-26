#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <assimp/vector2.h>
#include <assimp/postprocess.h>
#include "engine/vfs.h"
#include "core/io/logger.h"
#include "graphics/assets/model.h"
#include "core/memory/linear_allocator.h"

struct ModelLoadingScratch {
	uint mesh_count;
	uint vertices_count;
	uint indices_count;
	uint lod;
	
	
	const aiScene* scene;
	glm::mat4 apply_trans;
	Mesh* meshes_base;
	Vertex* vertex_base;
	uint* indices_base;
};


 void process_Mesh(ModelLoadingScratch* scratch, aiMesh* mesh) {
	glm::mat4 apply_trans = scratch->apply_trans;
	glm::mat3 apply_trans3 = scratch->apply_trans;
	 
	//SUB ALLOCATE VERTICES
	uint vertices_count = mesh->mNumVertices;
	Vertex* vertices = scratch->vertex_base + scratch->vertices_count;
	scratch->vertices_count += vertices_count;

	//FILL VERTICES
	AABB aabb;

	auto coords = mesh->mTextureCoords[0];

	for (int i = 0; i < mesh->mNumVertices; i++) {
		auto position = mesh->mVertices[i];

		auto tangent = mesh->mTangents[i];
		auto bitangent = mesh->mBitangents[i];

		auto first_coords = coords ? glm::vec2(coords[i].x, coords[i].y) : glm::vec2(0,0);
		auto normals = mesh->mNormals[i];

		vertices[i] = {
			apply_trans * glm::vec4(position.x, position.y, position.z, 1.0),
			apply_trans3 * glm::vec3(normals.x, normals.y, normals.z),
			first_coords,
			apply_trans3 * glm::vec3(tangent.x, tangent.y, tangent.z),
			apply_trans3 * glm::vec3(bitangent.x, bitangent.y, bitangent.z)
		};

		aabb.update(vertices[i].position);
	}

	//FILL INDICES

	uint* indices = scratch->indices_base + scratch->indices_count;
	uint indices_count = 0;

	//TODO could assume them to always be three
	for (int i = 0; i < mesh->mNumFaces; i++) {
		auto face = mesh->mFaces[i];
		memcpy(indices + indices_count, face.mIndices, sizeof(unsigned int) * face.mNumIndices);
		indices_count += face.mNumIndices;
	}

	scratch->indices_count += indices_count;

	Mesh* new_mesh = scratch->meshes_base + scratch->mesh_count;
	new_mesh->vertices[scratch->lod] = { vertices, vertices_count };
	new_mesh->indices[scratch->lod] = { indices, indices_count };
	new_mesh->aabb = aabb;
	new_mesh->material_id = mesh->mMaterialIndex;

	if (!coords) new_mesh->flags |= MESH_WITH_NO_UVS;

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

void load_assimp(Model* model, string_view real_path, const glm::mat4& apply_transform) { //TODO WHEN REIMPORTED, can reuse same memory
	Assimp::Importer importer[MAX_MESH_LOD]; //todo algorithm could probably be rearranged so that keeping all the lods alive in memory at the same time is not necessary
	array<MAX_MESH_LOD, const aiScene*> lods;

	string_view pre = ".fbx";

	if (real_path.ends_with(".fbx")) {
		string_view trimmed = real_path.sub(0, real_path.length - 4);
		if (trimmed.ends_with("_lod0")) {
			trimmed = trimmed.sub(0, trimmed.length - 5);
		}

		for (uint i = 0; i < MAX_MESH_LOD; i++) {
			string_buffer path = tformat(trimmed, "_lod", i, ".fbx");
			auto scene = importer[i].ReadFile(path.c_str(), aiProcess_Triangulate | aiProcess_CalcTangentSpace);
			if (!scene) break;

			lods.append(scene);
		}
	}

	if (lods.length == 0) {
		auto scene = importer[0].ReadFile(real_path.c_str(), aiProcess_Triangulate | aiProcess_CalcTangentSpace);
		if (!scene) throw string_buffer("Could not load model ") + real_path;
	
		lods.append(scene);
	}

	//todo validate that all lods have the same number of meshes and materials!
	ModelLoadingScratch scratch = {};
	scratch.mesh_count = lods[0]->mNumMeshes;

	for (const aiScene* scene : lods) {
		process_size(scene->mRootNode, scene, &scratch);
	}

	scratch.meshes_base  = PERMANENT_ARRAY(Mesh,   scratch.mesh_count);
	scratch.vertex_base  = PERMANENT_ARRAY(Vertex, scratch.vertices_count);
	scratch.indices_base = PERMANENT_ARRAY(uint,   scratch.indices_count);


	printf("\nLoading model %s\n", real_path.data);
	printf("\tMeshes: %i\n", scratch.mesh_count);
	printf("\tVertices: %i\n", scratch.vertices_count);
	printf("\tIndices: %i\n", scratch.indices_count);

	scratch.vertices_count = 0;
	scratch.indices_count = 0;
	scratch.apply_trans = apply_transform;

	for (const aiScene* scene : lods) {
		scratch.mesh_count = 0;
		scratch.scene = scene;
		process_Node(&scratch, scene->mRootNode);
		scratch.lod++;
	}

	model->meshes.length = scratch.mesh_count;
	model->meshes.data = scratch.meshes_base;
	model->aabb = AABB();
	
	//todo merge into one upload
	for (int i = 0; i < scratch.mesh_count; i++) {
		Mesh* mesh = scratch.meshes_base + i;
		mesh->lod_count = lods.length;

		model->aabb.update_aabb(mesh->aabb);

		for (uint lod = 0; lod < lods.length; lod++) {
			mesh->buffer[lod] = alloc_vertex_buffer<Vertex>(VERTEX_LAYOUT_DEFAULT, mesh->vertices[lod], mesh->indices[lod]);
		}
	}

	model->materials.length = lods[0]->mNumMaterials;
	model->materials.data = PERMANENT_ARRAY(sstring, model->materials.length);
	
	
	for (int i = 0; i < model->materials.length; i++) {
		auto aMat = lods[0]->mMaterials[i];
		aiString c_name;
		aiGetMaterialString(aMat, AI_MATKEY_NAME, &c_name);

		printf("Material name %s\n", c_name.data);

		model->materials[i] = c_name.data;
	}

	float MESH_CULL_DISTANCE = 100.0f;

	uint dist_per_lod = MESH_CULL_DISTANCE / lods.length;
	
	if (model->lod_distance.length == 0) {

		//todo this is a pretty shitty distribution
		for (int i = 0; i < lods.length; i++) {
			model->lod_distance.append((i + 1) * dist_per_lod);
		}
	}
	else {
		int diff = lods.length - model->lod_distance.length;
		int last_lod = model->lod_distance.last();

		for (int i = 0; i < diff; i++) {
			uint lod_dist = last_lod * 1.5;
			model->lod_distance.append(lod_dist);
			last_lod = lod_dist;
		}

		for (int i = 0; i < -diff; i++) lods.pop();
	}
}