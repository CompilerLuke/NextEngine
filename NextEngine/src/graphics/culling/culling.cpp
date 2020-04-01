#include "stdafx.h"
#include "graphics/culling/culling.h"
#include <glm/vec4.hpp>
#include <glm/glm.hpp>
#include "graphics/renderer/renderer.h"
#include "components/transform.h"
#include "graphics/assets/asset_manager.h"
#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/renderer.h"
#include "graphics/renderer/material_system.h"
#include "components/grass.h"
#include "core/io/logger.h"

void aabb_to_verts(AABB* self, glm::vec4* verts) {
	verts[0] = glm::vec4(self->max.x, self->max.y, self->max.z, 1);
	verts[1] = glm::vec4(self->min.x, self->max.y, self->max.z, 1);
	verts[2] = glm::vec4(self->max.x, self->min.y, self->max.z, 1);
	verts[3] = glm::vec4(self->min.x, self->min.y, self->max.z, 1);

	verts[4] = glm::vec4(self->max.x, self->max.y, self->min.z, 1);
	verts[5] = glm::vec4(self->min.x, self->max.y, self->min.z, 1);
	verts[6] = glm::vec4(self->max.x, self->min.y, self->min.z, 1);
	verts[7] = glm::vec4(self->min.x, self->min.y, self->min.z, 1);
}

AABB AABB::apply(const glm::mat4& matrix) {
	AABB new_aabb;
	
	glm::vec4 verts[8];
	aabb_to_verts(this, verts);

	for (int i = 0; i < 8; i++) {
		glm::vec4 v = matrix * verts[i];
		new_aabb.update(v);
	}
	return new_aabb;
}

void AABB::update_aabb(AABB& other) {
	this->max = glm::max(this->max, other.max);
	this->min = glm::min(this->min, other.min);
}


void AABB::update(const glm::vec3& v) {
	this->max = glm::max(this->max, v);
	this->min = glm::min(this->min, v);
}

void extract_planes(RenderCtx& params, glm::vec4 planes[6]) {
	glm::mat4 mat = params.projection * params.view;
	
	for (int i = 0; i < 4; i++) planes[0][i] = mat[i][3] + mat[i][0];
	for (int i = 0; i < 4; i++) planes[1][i] = mat[i][3] - mat[i][0];
	for (int i = 0; i < 4; i++) planes[2][i] = mat[i][3] + mat[i][1];
	for (int i = 0; i < 4; i++) planes[3][i] = mat[i][3] - mat[i][1];
	for (int i = 0; i < 4; i++) planes[4][i] = mat[i][3] + mat[i][2];
	for (int i = 0; i < 4; i++) planes[5][i] = mat[i][3] - mat[i][2];
}
CullResult frustum_test(glm::vec4 planes[6], const AABB& aabb) {
	CullResult result = INSIDE;
	return INSIDE;

	for (int planeID = 0; planeID < 6; planeID++) {
		glm::vec3 planeNormal(planes[planeID]);
		float planeConstant(planes[planeID].w);

		glm::vec3 vmin, vmax;

		if (planes[planeID].x < 0) {
			vmin.x = aabb.min.x;
			vmax.x = aabb.max.x;
		}
		else {
			vmin.x = aabb.max.x;
			vmax.x = aabb.min.x;
		}

		if (planes[planeID].y < 0) {
			vmin.y = aabb.min.y;
			vmax.y = aabb.max.y;
		}
		else {
			vmin.y = aabb.max.y;
			vmax.y = aabb.min.y;
		}

		if (planes[planeID].z < 0) {
			vmin.z = aabb.min.z;
			vmax.z = aabb.max.z;
		}
		else {
			vmin.z = aabb.max.z;
			vmax.z = aabb.min.z;
		}

		if (glm::dot(planeNormal, vmin) + planeConstant < 0.0f) {
			return OUTSIDE;
		}

		//if (glm::dot(planeNormal, vmax) + planeConstant <= 0.0f) {
		//	result = INTERSECT;
		//}
	}

	return result;
}

#define MAX_MESHES_PER_NODE 10
#define MAX_DEPTH 7
//#define DEBUG_OCTREE

Node* subdivide_BVH(ScenePartition& scene_partition, AABB& node_aabb, int depth, int mesh_count, AABB* aabbs, int* meshes, glm::mat4* models_m) {
	assert(scene_partition.node_count < MAX_NODES);
	
	Node& node = scene_partition.nodes[scene_partition.node_count++];
	node = {};
	node.offset = scene_partition.meshes_count;

	if (mesh_count <= MAX_MESHES_PER_NODE || depth >= MAX_DEPTH) {
		node.aabb = node_aabb;
		node.count = mesh_count;
		
		memcpy(scene_partition.meshes    + node.offset, meshes,   sizeof(int) * node.count);
		memcpy(scene_partition.aabbs     + node.offset, aabbs,    sizeof(AABB) * node.count);
		memcpy(scene_partition.model_m   + node.offset, models_m, sizeof(glm::mat4) * node.count);

		assert(scene_partition.meshes_count <= MAX_MESH_INSTANCES);
	}
	else {
		AABB child_aabbs[2];
		vector<AABB> subdivided_aabbs[2];
		vector<int> subdivided_meshes[2];
		vector<glm::mat4> subdivided_model_m[2];

		int watermark = temporary_allocator.occupied;
		
		glm::vec3 size = node_aabb.max - node_aabb.min;
		int axis = size.x > size.y ? (size.x > size.z ? 0 : 2)  : (size.y > size.z ? 1 : 2);

		glm::vec3 pivot = 0.5f * (node_aabb.max + node_aabb.min);
		
		float half_size = size[axis] * 0.5f;

		//glm::vec3 half_size = 0.5f * (node.aabb.max - node.aabb.min);

		for (uint i = 0; i < 2; i++) {
			subdivided_aabbs[i].allocator = &temporary_allocator;
			subdivided_meshes[i].allocator = &temporary_allocator;
			subdivided_model_m[i].allocator = &temporary_allocator;
		}

		for (int i = 0; i < mesh_count; i++) {
			AABB& aabb = aabbs[i];
			glm::vec3 centroid = (aabb.min + aabb.max) / 2.0f;
			glm::vec3 size = aabb.max - aabb.min;

			if (size[axis] > half_size) {
				int offset = scene_partition.meshes_count++;

				assert(scene_partition.meshes_count <= MAX_MESH_INSTANCES);

				node.count++;
				scene_partition.aabbs[offset] = aabb;
				scene_partition.meshes[offset] = meshes[i];
				scene_partition.model_m[offset] = models_m[i];

				node.aabb.update_aabb(aabb);

				continue;
			}


			bool past_x_pivot = centroid.x > pivot.x;
			bool past_y_pivot = centroid.y > pivot.y;
			bool past_z_pivot = centroid.z > pivot.z;

			bool node_index = centroid[axis] > pivot[axis];
				
				//past_x_pivot + past_y_pivot * 2 + past_z_pivot * 4; 

			child_aabbs[node_index].update_aabb(aabb);

			subdivided_aabbs[node_index].append(aabb);
			subdivided_meshes[node_index].append(meshes[i]);
			subdivided_model_m[node_index].append(models_m[i]);
		}

#ifdef DEBUG_OCTREE
		for (int i = 0; i < 8; i++) {
			int value = i;
			bool past_z_pivot = value / 4;
			value -= past_z_pivot * 4;
			bool past_y_pivot = value / 2;
			bool past_x_pivot = value % 2;
				
			if (past_z_pivot) child_aabbs[i].min.z = node_aabb.min.z + half_size.z;
			else child_aabbs[i].min.z = node_aabb.min.z;

			if (past_y_pivot) child_aabbs[i].min.y = node_aabb.min.y + half_size.y;
			else child_aabbs[i].min.y = node_aabb.min.y ;

			if (past_x_pivot) child_aabbs[i].min.x = node_aabb.min.x + half_size.x;
			else child_aabbs[i].min.x = node_aabb.min.x;

			child_aabbs[i].max = child_aabbs[i].min + half_size;
		}
#endif

		for (int i = 0; i < 2; i++) {
			if (subdivided_aabbs[i].length == 0) continue;
			node.child[i] = subdivide_BVH(scene_partition, child_aabbs[i], depth + 1, subdivided_aabbs[i].length, subdivided_aabbs[i].data, subdivided_meshes[i].data, subdivided_model_m[i].data);
			node.aabb.update_aabb(node.child[i]->aabb);
		}

		temporary_allocator.occupied = watermark;
	}

	scene_partition.meshes_count += node.count;

	return &node;
}

void build_acceleration_structure(ScenePartition& scene_partition, hash_set<MeshBucket, MAX_MESH_BUCKETS> & mesh_buckets, ModelManager& model_manager, World& world) { //todo UGH THE CURRENT SYSTEM LIMITS HOW DATA ORIENTED THIS CAN BE
	AABB world_bounds;	
	
	int occupied = temporary_allocator.occupied;

	vector<AABB> aabbs; 
	vector<int> meshes; 
	vector<glm::mat4> models_m; 
	
	aabbs.allocator = &temporary_allocator;
	meshes.allocator = &temporary_allocator;
	models_m.allocator = &temporary_allocator;

	for (ID id : world.filter<ModelRenderer, Materials, Transform>(ANY_LAYER)) {
		auto trans = world.by_id<Transform>(id);
		auto model_renderer = world.by_id<ModelRenderer>(id);
		auto materials = world.by_id<Materials>(id);

		Model* model = model_manager.get(model_renderer->model_id);
		glm::mat4 model_m = trans->compute_model_matrix();

		if (model == NULL) continue;

		AABB aabb = model->aabb.apply(model_m);

		world_bounds.update_aabb(aabb);

		for (int mesh_index = 0; mesh_index < model->meshes.length; mesh_index++) {
			Mesh& mesh = model->meshes[mesh_index];
			material_handle mat_handle = materials->materials[mesh.material_id];

			MeshBucket bucket;
			bucket.model_id = model_renderer->model_id;
			bucket.mesh_id = mesh_index;
			bucket.mat_id = mat_handle;

			aabbs.append(mesh.aabb.apply(model_m));
			meshes.append(mesh_buckets.add(bucket));
			models_m.append(model_m);
		}
	}

	for (ID id : world.filter<Transform, Materials, Grass>(ANY_LAYER)) {
		auto trans = world.by_id<Transform>(id);
		auto grass = world.by_id<Grass>(id);
		auto materials = world.by_id<Materials>(id);

		Model* model = model_manager.get(grass->placement_model);
		
		if (model == NULL) continue;

		AABB aabb;
		aabb.min = -0.5f * glm::vec3(grass->width, 0, grass->height);
		aabb.max = 0.5f * glm::vec3(grass->width, grass->max_height, grass->height);
		
		aabb.min.x += trans->position.x;
		aabb.min.z += trans->position.z;
		aabb.max.x += trans->position.x;
		aabb.max.z += trans->position.z;

		world_bounds.update_aabb(aabb);

		for (int mesh_index = 0; mesh_index < model->meshes.length; mesh_index++) {
			Mesh& mesh = model->meshes[mesh_index];
			material_handle mat_handle = materials->materials[mesh.material_id];

			MeshBucket bucket;
			bucket.model_id = grass->placement_model;
			bucket.mesh_id = mesh_index;
			bucket.mat_id = mat_handle;
			
			int bucket_id = mesh_buckets.add(bucket);

			for (Transform& trans : grass->transforms) {
				glm::mat4 model_m = trans.compute_model_matrix();

				aabbs.append(mesh.aabb.apply(model_m));
				meshes.append(bucket_id);
				models_m.append(model_m);
			}
		}
	}

	log("==================");

	subdivide_BVH(scene_partition, world_bounds, 0, aabbs.length, aabbs.data, meshes.data, models_m.data);

	temporary_allocator.occupied = occupied;
}

CullingSystem::CullingSystem(AssetManager& asset_manager, ModelRendererSystem& model_renderer, World& world) 
	: model_renderer(model_renderer), asset_manager(asset_manager) {
	/*world.on_make<Transform, ModelRenderer>([&world](vector<ID>& ids) {
		for (ID id : ids) {
			auto trans = world.by_id<Transform>(id);
			auto model_renderer = world.by_id<ModelRenderer>(id);
		}
	});
	*/
}

#define MAX_DEPTH_CHECK_EACH 5


void cull_node(CulledMeshBucket* culled, ScenePartition& partition, Node& node, int depth, glm::vec4 planes[6]) {
	CullResult cull_result = frustum_test(planes, node.aabb);
	if (cull_result == OUTSIDE) return;
	
	bool test_children = depth < MAX_DEPTH_CHECK_EACH;

	for (int i = node.offset; i < node.offset + node.count; i++) {
		if (test_children && frustum_test(planes, partition.aabbs[i]) == OUTSIDE) continue;

		culled[partition.meshes[i]].model_m.append(partition.model_m[i]);
	}

	for (int i = 0; i < 2; i++) {
		if (node.child[i]) cull_node(culled, partition, *node.child[i], depth, planes);
	}
}

void CullingSystem::cull(World& world, RenderCtx& ctx) {
	if (scene_partition.node_count == 0) {
		build_acceleration(world);
	}

	glm::vec4 planes[6];
	extract_planes(ctx, planes);

	CulledMeshBucket* culled_mesh_bucket = model_renderer.pass_culled_bucket[ctx.pass->id];

	for (int i = 0; i < MAX_MESH_BUCKETS; i++) {
		culled_mesh_bucket[i].model_m.clear();
	}

	cull_node(culled_mesh_bucket, scene_partition, scene_partition.nodes[0], 0, planes);
}

void CullingSystem::build_acceleration(World& world) {
	build_acceleration_structure(scene_partition, model_renderer.mesh_buckets, asset_manager.models, world);
}

void render_node(RenderCtx& ctx, Material* mat, Model* cube, Node& node) {
	Transform trans;
	trans.position = (node.aabb.max + node.aabb.min) * 0.5f;
	trans.scale = node.aabb.max - node.aabb.min;
	trans.scale *= 0.5f;

	glm::mat4 model_m = trans.compute_model_matrix();

	ctx.command_buffer.draw(model_m, &cube->meshes[0].buffer, mat);

	for (int i = 0; i < 2; i++) {
		if (node.child[i]) render_node(ctx, mat, cube, *node.child[i]);
	}
}

#include "graphics/renderer/material_system.h"

DrawCommandState draw_wireframe_state = default_draw_state;

void CullingSystem::render_debug_bvh(World& world, RenderCtx& ctx) {
	return;

	ModelManager& models = asset_manager.models;
	ShaderManager& shaders = asset_manager.shaders;

	model_handle cube = models.load("cube.fbx");

	draw_wireframe_state.mode = DrawWireframe;

	Material* mat = TEMPORARY_ALLOC(Material);
	mat->shader = shaders.load("shaders/pbr.vert", "shaders/gizmo.frag");
	mat->set_vec3(shaders, "color", glm::vec3(1.0f, 0.0f, 0.0f));
	mat->state = &draw_wireframe_state;

	render_node(ctx, mat, models.get(cube), scene_partition.nodes[0]);
}