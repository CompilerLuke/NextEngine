#include "graphics/culling/culling.h"
#include <glm/vec4.hpp>
#include <glm/glm.hpp>
#include "graphics/renderer/renderer.h"
#include "graphics/rhi/pipeline.h"
#include "components/transform.h"
#include "graphics/assets/assets.h"
#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/material.h"
#include "components/grass.h"
#include "core/io/logger.h"
#include "core/container/tvector.h"
#include "core/profiler.h"

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

//could cache this result in viewport
void extract_planes(const Viewport& viewport, glm::vec4 planes[6]) {
	glm::mat4 mat = viewport.proj * viewport.view;
	
	for (int i = 0; i < 4; i++) planes[0][i] = mat[i][3] + mat[i][0];
	for (int i = 0; i < 4; i++) planes[1][i] = mat[i][3] - mat[i][0];
	for (int i = 0; i < 4; i++) planes[2][i] = mat[i][3] + mat[i][1];
	for (int i = 0; i < 4; i++) planes[3][i] = mat[i][3] - mat[i][1];
	for (int i = 0; i < 4; i++) planes[4][i] = mat[i][3] + mat[i][2];
	for (int i = 0; i < 4; i++) planes[5][i] = mat[i][3] - mat[i][2];
}

CullResult frustum_test(glm::vec4 planes[6], const AABB& aabb) {
	CullResult result = INSIDE;

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

	//return INSIDE;
	return result;
}

#define MAX_MESHES_PER_NODE 10
#define MAX_DEPTH 7
//#define DEBUG_OCTREE

Node& alloc_node(Partition& scene_partition) {
	assert(scene_partition.count <= MAX_MESH_INSTANCES);

	Node& node = scene_partition.nodes[scene_partition.node_count++];
	node = {};
	node.offset = scene_partition.count;

	return node;
}

Node& alloc_leaf_node(Partition& scene_partition, AABB& node_aabb, int max, int count) {
	Node& node = alloc_node(scene_partition);
	node.aabb = node_aabb;
	node.count = count;
	
	scene_partition.count += count;

	assert(scene_partition.count <= max);

	return node;
}

BranchNodeInfo alloc_branch_node(Partition& scene_partition, AABB& node_aabb) {
	BranchNodeInfo info = { alloc_node(scene_partition) };
	glm::vec3 size = node_aabb.size();

	info.watermark = temporary_allocator.occupied;
	info.axis = size.x > size.y ? (size.x > size.z ? 0 : 2) : (size.y > size.z ? 1 : 2);
	info.pivot = 0.5f * node_aabb.centroid();
	info.half_size = size[info.axis] * 0.5f;

	return info;
}

int elem_offset(Partition& scene_partition, BranchNodeInfo& info, AABB& aabb) {
	int offset = scene_partition.count++;
	assert(scene_partition.count <= MAX_MESH_INSTANCES);
	info.node.aabb.update_aabb(aabb);
	info.node.count++;

	return offset;
}


bool bigger_than_leaf(BranchNodeInfo& info, AABB& aabb) {
	return aabb.size()[info.axis] > info.half_size;
}

int split_index(BranchNodeInfo& info, AABB& aabb) {
	int node_index = aabb.centroid()[info.axis] > info.pivot[info.axis];
	info.child_aabbs[node_index].update_aabb(aabb);
	return node_index;
}

void bump_allocator(Partition& partition, BranchNodeInfo& info) {
	temporary_allocator.occupied = info.watermark;
	partition.count += info.node.count;
}

Node* subdivide_BVH(ScenePartition& scene_partition, AABB& node_aabb, int depth, int mesh_count, AABB* aabbs, int* meshes, glm::mat4* models_m) {
	if (mesh_count <= MAX_MESHES_PER_NODE || depth >= MAX_DEPTH) {
		Node& node = alloc_leaf_node(scene_partition, node_aabb, MAX_MESH_INSTANCES, mesh_count);

		copy_into_node(node, scene_partition.meshes, meshes);
		copy_into_node(node, scene_partition.aabbs, aabbs);
		copy_into_node(node, scene_partition.model_m, models_m);

		return &node;
	}
	else {
		BranchNodeInfo info = alloc_branch_node(scene_partition, node_aabb);

		tvector<AABB> subdivided_aabbs[2];
		tvector<int> subdivided_meshes[2];
		tvector<glm::mat4> subdivided_model_m[2];

		for (int i = 0; i < mesh_count; i++) {
			AABB& aabb = aabbs[i];

			if (bigger_than_leaf(info, aabb)) {
				int offset = elem_offset(scene_partition, info, aabb);

				scene_partition.aabbs[offset] = aabbs[i];
				scene_partition.meshes[offset] = meshes[i];
				scene_partition.model_m[offset] = models_m[i];
			}
			else {
				bool node_index = split_index(info, aabb);

				subdivided_aabbs[node_index].append(aabb);
				subdivided_meshes[node_index].append(meshes[i]);
				subdivided_model_m[node_index].append(models_m[i]);
			}
		}

		for (int i = 0; i < 2; i++) {
			if (subdivided_aabbs[i].length == 0) continue;
			info.node.child[i] = subdivide_BVH(scene_partition, info.child_aabbs[i], depth + 1, subdivided_aabbs[i].length, subdivided_aabbs[i].data, subdivided_meshes[i].data, subdivided_model_m[i].data);
			info.node.aabb.update_aabb(info.node.child[i]->aabb);
		}

		bump_allocator(scene_partition, info);
		return &info.node;
	}
}

void build_acceleration_structure(ScenePartition& scene_partition, hash_set<MeshBucket, MAX_MESH_BUCKETS> & mesh_buckets, World& world) { //todo UGH THE CURRENT SYSTEM LIMITS HOW DATA ORIENTED THIS CAN BE
	Profile profile("Build Acceleration");
	
	AABB world_bounds;	
	
	int occupied = temporary_allocator.occupied;

	vector<AABB> aabbs; 
	vector<int> meshes; 
	vector<glm::mat4> models_m; 
	
	aabbs.allocator = &temporary_allocator;
	meshes.allocator = &temporary_allocator;
	models_m.allocator = &temporary_allocator;

	for (auto [e,trans,model_renderer,materials] : world.filter<Transform,ModelRenderer, Materials>(ANY_LAYER)) {
		Model* model = get_Model(model_renderer.model_id);
		glm::mat4 model_m = compute_model_matrix(trans);

		if (model == NULL) continue;

		AABB aabb = model->aabb.apply(model_m);

		world_bounds.update_aabb(aabb);

		for (int mesh_index = 0; mesh_index < model->meshes.length; mesh_index++) {
			Mesh& mesh = model->meshes[mesh_index];
			material_handle mat_handle = materials.materials[mesh.material_id];
			MaterialDesc* mat_desc = material_desc(mat_handle);

			MeshBucket bucket;
			bucket.model_id = model_renderer.model_id;
			bucket.mesh_id = mesh_index;
			bucket.mat_id = mat_handle;
			bucket.flags = CAST_SHADOWS;

			//todo support shadow passes RenderPass::ScenePassCount

			for (uint pass = 0; pass < 1; pass++) {
				PipelineDesc pipeline_desc;
				pipeline_desc.shader = mat_desc->shader;
				pipeline_desc.state = mat_desc->draw_state;
				pipeline_desc.vertex_layout = VERTEX_LAYOUT_DEFAULT;
				pipeline_desc.instance_layout = INSTANCE_LAYOUT_MAT4X4;
				pipeline_desc.render_pass = render_pass_by_id((RenderPass::ID)pass);

				bucket.pipeline_id[pass] = query_Pipeline(pipeline_desc);
			}

			aabbs.append(mesh.aabb.apply(model_m));
			meshes.append(mesh_buckets.add(bucket));
			models_m.append(model_m);
		}
	}

	for (auto [e,trans,grass,materials] : world.filter<Transform, Grass, Materials>(ANY_LAYER)) {
		Model* model = get_Model(grass.placement_model);
		
		if (model == NULL) continue;

		AABB aabb;
		aabb.min = -0.5f * glm::vec3(grass.width, 0, grass.height);
		aabb.max = 0.5f * glm::vec3(grass.width, grass.max_height, grass.height);
		
		aabb.min.x += trans.position.x;
		aabb.min.z += trans.position.z;
		aabb.max.x += trans.position.x;
		aabb.max.z += trans.position.z;

		world_bounds.update_aabb(aabb);

		for (int mesh_index = 0; mesh_index < model->meshes.length; mesh_index++) {
			Mesh& mesh = model->meshes[mesh_index];
			material_handle mat_handle = materials.materials[mesh.material_id];

			MeshBucket bucket;
			bucket.model_id = grass.placement_model;
			bucket.mesh_id = mesh_index;
			bucket.mat_id = mat_handle;
			bucket.flags = grass.cast_shadows ? CAST_SHADOWS : 0;
			
			int bucket_id = mesh_buckets.add(bucket);

			for (Transform& trans : grass.transforms) {
				glm::mat4 model_m = compute_model_matrix(trans);

				aabbs.append(mesh.aabb.apply(model_m));
				meshes.append(bucket_id);
				models_m.append(model_m);
			}
		}
	}

	subdivide_BVH(scene_partition, world_bounds, 0, aabbs.length, aabbs.data, meshes.data, models_m.data);

	temporary_allocator.occupied = occupied;
}


#define MAX_DEPTH_CHECK_EACH 5

void cull_node(CulledMeshBucket* culled, const ScenePartition& partition, const Node& node, int depth, glm::vec4 planes[6]) {
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

void update_acceleration_structure(ScenePartition& scene_partition, hash_set<MeshBucket, MAX_MESH_BUCKETS> & mesh_buckets, World& world) {
	if (scene_partition.node_count == 0) {
		build_acceleration_structure(scene_partition, mesh_buckets, world);
	}
}

void cull_meshes(const ScenePartition& scene_partition, CulledMeshBucket* culled_mesh_bucket, const Viewport& viewport) {
	glm::vec4 planes[6];
	extract_planes(viewport, planes);

	for (int i = 0; i < MAX_MESH_BUCKETS; i++) {
		culled_mesh_bucket[i].model_m.clear();
	}

	cull_node(culled_mesh_bucket, scene_partition, scene_partition.nodes[0], 0, planes);
}

void render_node(RenderPass& ctx, material_handle mat, model_handle cube, Node& node) {
	Transform trans;
	trans.position = (node.aabb.max + node.aabb.min) * 0.5f;
	trans.scale = node.aabb.max - node.aabb.min;
	trans.scale *= 0.5f;

	draw_mesh(ctx.cmd_buffer, cube, mat, trans);

	for (int i = 0; i < 2; i++) {
		if (node.child[i]) render_node(ctx, mat, cube, *node.child[i]);
	}
}

#include "graphics/assets/material.h"

DrawCommandState draw_wireframe_state = PolyMode_Wireframe;

void render_debug_bvh(ScenePartition& scene_partition, RenderPass& ctx) {
	return; //todo make toggle in editor

	model_handle cube = load_Model("cube.fbx");

	//Material* mat = TEMPORARY_ALLOC(Material);
	//mat->shader = load(shaders, "shaders/pbr.vert", "shaders/gizmo.frag");
	//mat->set_vec3(shaders, "color", glm::vec3(1.0f, 0.0f, 0.0f));
	//mat->state = &draw_wireframe_state;

	//render_node(ctx, mat, models.get(cube), scene_partition.nodes[0]);
}