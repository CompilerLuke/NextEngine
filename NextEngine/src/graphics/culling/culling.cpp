#include "graphics/culling/culling.h"
#include "graphics/culling/build_bvh.h"
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
#include "ecs/ecs.h"
#include "core/job_system/job.h"
#include "core/memory/allocator.h"

//could cache this result in viewport
void extract_planes(Viewport& viewport) {
	glm::mat4 mat = viewport.proj * viewport.view;
	
	for (int i = 0; i < 4; i++) viewport.frustum_planes[0][i] = mat[i][3] + mat[i][0];
	for (int i = 0; i < 4; i++) viewport.frustum_planes[1][i] = mat[i][3] - mat[i][0];
	for (int i = 0; i < 4; i++) viewport.frustum_planes[2][i] = mat[i][3] + mat[i][1];
	for (int i = 0; i < 4; i++) viewport.frustum_planes[3][i] = mat[i][3] - mat[i][1];
	for (int i = 0; i < 4; i++) viewport.frustum_planes[4][i] = mat[i][3] + mat[i][2]; //front
	for (int i = 0; i < 4; i++) viewport.frustum_planes[5][i] = mat[i][3] - mat[i][2]; //back
}

CullResult frustum_test(const glm::vec4 planes[6], const AABB& aabb) {	
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

	return result;
}

#define MAX_MESHES_PER_NODE 10
#define MAX_DEPTH 5
//#define DEBUG_OCTREE

struct SubdivideBVHJob {
	ScenePartition* scene_partition;
	AABB node_aabb;
	uint depth;
	uint mesh_count;
	AABB* aabbs;
	int* meshes;
	glm::mat4* models_m;
	uint child_idx;
};

void subdivide_BVH(SubdivideBVHJob& job) {
	ScenePartition& scene_partition = *job.scene_partition;
	
	if (job.mesh_count <= MAX_MESHES_PER_NODE || job.depth >= MAX_DEPTH) {
		Node& node = alloc_leaf_node(scene_partition, job.node_aabb, MAX_MESH_INSTANCES, job.mesh_count);
	
		copy_into_node(node, scene_partition.meshes, job.meshes);
		copy_into_node(node, scene_partition.aabbs, job.aabbs);
		copy_into_node(node, scene_partition.model_m, job.models_m);

		job.child_idx = &node - scene_partition.nodes;
	}
	else {
		LinearAllocator& temporary = get_thread_local_temporary_allocator();
        LinearRegion region(temporary);
        
		BranchNodeInfo info = alloc_branch_node(scene_partition, job.node_aabb);

		tvector<AABB> subdivided_aabbs[2];
		tvector<int> subdivided_meshes[2];
		tvector<glm::mat4> subdivided_model_m[2];

		for (uint i = 0; i < 2; i++) {
			subdivided_aabbs[i].allocator = &temporary;
			subdivided_meshes[i].allocator = &temporary;
			subdivided_model_m[i].allocator = &temporary;
		}

        int* split_idx = alloc_t<int>(temporary, job.mesh_count);
        uint offset = split_aabbs(scene_partition, info, {job.aabbs, job.mesh_count}, split_idx);
        
		for (int i = 0; i < job.mesh_count; i++) {
            uint node_index = split_idx[i];

			if (node_index == -1) {
				scene_partition.aabbs[offset] = job.aabbs[i];
				scene_partition.meshes[offset] = job.meshes[i];
				scene_partition.model_m[offset] = job.models_m[i];
                offset++;
			}
			else {
				subdivided_aabbs[node_index].append(job.aabbs[i]);
				subdivided_meshes[node_index].append(job.meshes[i]);
				subdivided_model_m[node_index].append(job.models_m[i]);
			}
		}

		JobDesc desc[2];
        SubdivideBVHJob jobs[2];

		uint count = 0;
		for (int i = 0; i < 2; i++) {
			if (subdivided_aabbs[i].length == 0) continue;
			
            jobs[count].scene_partition = job.scene_partition;
            jobs[count].node_aabb = info.child_aabbs[i];
			jobs[count].depth = job.depth + 1;
			jobs[count].mesh_count = subdivided_aabbs[i].length;
			jobs[count].aabbs = subdivided_aabbs[i].data;
			jobs[count].meshes = subdivided_meshes[i].data;
			jobs[count].models_m = subdivided_model_m[i].data;

			desc[count] = {subdivide_BVH, jobs + i};
			count++;
		}

		wait_for_jobs(PRIORITY_HIGH, { desc, count });

		for (int i = 0; i < 2; i++) {
			uint child_idx = jobs[i].child_idx;
			info.node.child[info.node.child_count++] = child_idx;
			info.node.aabb.update_aabb(scene_partition.nodes[child_idx].aabb);
		}

        info.node.child_count = count;
		job.child_idx = &info.node - scene_partition.nodes;
	}
}

void assign_meshes_to_buckets(
    World& world,
    hash_set<MeshBucket, MAX_MESH_BUCKETS>& mesh_buckets,
    tvector<AABB>& aabbs,
    tvector<glm::mat4>& models_m,
    tvector<int>& meshes,
    EntityQuery query
) {
    for (auto [e,trans,model_renderer,materials] : world.filter<Transform,ModelRenderer, Materials>(query)) {
        Model* model = get_Model(model_renderer.model_id);
        glm::mat4 model_m = compute_model_matrix(trans);

        if (model == NULL) continue;

        for (int mesh_index = 0; mesh_index < model->meshes.length; mesh_index++) {
            Mesh& mesh = model->meshes[mesh_index]; //todo extend for lods

			material_handle mat_handle = mat_by_index(materials, mesh.material_id);

            MeshBucket bucket;
            bucket.model = model_renderer.model_id;
            bucket.mesh_id = mesh_index;
            bucket.mat = mat_handle;
            bucket.flags = CAST_SHADOWS;

            //todo support shadow passes RenderPass::ScenePassCount

			GraphicsPipelineDesc shadow_pipeline_desc;
			mat_pipeline_desc(shadow_pipeline_desc, mat_handle, RenderPass::Shadow0, 0);
			shadow_pipeline_desc.state = Cull_None | DynamicState_DepthBias;

            bucket.depth_only_pipeline = query_Pipeline(shadow_pipeline_desc);
			bucket.depth_prepass = query_pipeline(mat_handle, RenderPass::Scene, 0);
            bucket.color_pipeline = query_pipeline(mat_handle, RenderPass::Scene, 1);

            aabbs.append(mesh.aabb.apply(model_m));
            meshes.append(mesh_buckets.add(bucket));
            models_m.append(model_m);
        }
    }

}

void build_acceleration_structure(ScenePartition& scene_partition, hash_set<MeshBucket, MAX_MESH_BUCKETS> & mesh_buckets, World& world) { //todo UGH THE CURRENT SYSTEM LIMITS HOW DATA ORIENTED THIS CAN BE
	Profile profile("Build Acceleration");
	
	AABB world_bounds;	
	LinearAllocator& temporary_allocator = get_temporary_allocator();
    LinearRegion linear_region(temporary_allocator);

	tvector<AABB> aabbs; 
	tvector<int> meshes; 
	tvector<glm::mat4> models_m; 
	
	aabbs.allocator = &temporary_allocator;
	meshes.allocator = &temporary_allocator;
	models_m.allocator = &temporary_allocator;
    
    assign_meshes_to_buckets(world, mesh_buckets, aabbs, models_m, meshes, {STATIC});
    
    for (AABB& aabb : aabbs) world_bounds.update_aabb(aabb);

	SubdivideBVHJob job = {&scene_partition, world_bounds};
	job.depth = 0;
	job.mesh_count = aabbs.length;
	job.aabbs = aabbs.data;
	job.meshes = meshes.data;
	job.models_m = models_m.data;

	subdivide_BVH(job);
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

	for (int i = 0; i < node.child_count; i++) {
		cull_node(culled, partition, partition.nodes[node.child[i]], depth, planes);
	}
}

void update_acceleration_structure(ScenePartition& scene_partition, MeshBuckets& mesh_buckets, World& world) {
	if (scene_partition.node_count == 0) {
		build_acceleration_structure(scene_partition, mesh_buckets, world);
	}
}

/*
void cull_dynamic_meshes(const ScenePartition& scene_partition, World& world, MeshBuckets& mesh_buckets, CulledMeshBucket* culled_mesh_bucket, glm::vec4 planes[6]) {
    
    tvector<AABB> aabbs;
    tvector<glm::mat4> model_m;
    tvector<int> meshes;
    
    assign_buckets_to_meshes(world, mesh_buckets, aabbs, model_m, meshes, {0,0,STATIC});
    
    for (uint i = 0; i < meshes.length; i++) {
        if (frustum_test(planes, aabbs[i]) == OUTSIDE) continue;
        culled_mesh_bucket[meshes[i]].model_m.append(model_m[i]);
    }
}

void cull_static_meshes(const ScenePartition& scene_partition, CulledMeshBucket* culled_mesh_bucket, glm::vec4 planes[6]) {
    cull_node(culled_mesh_bucket, scene_partition, scene_partition.nodes[0], 0, planes);
}
*/

struct CullMeshJob {
	const ScenePartition* partition;
	MeshBuckets* buckets;
	slice<AABB> aabbs;
	slice<glm::mat4> model_m;
	slice<int> meshes;
	glm::vec4* planes;
	CulledMeshBucket* result;
};

void cull_mesh_job(CullMeshJob& job) {
	for (int i = 0; i < MAX_MESH_BUCKETS; i++) {
		job.result[i].model_m.clear();
	}

	for (uint i = 0; i < job.meshes.length; i++) {
		if (frustum_test(job.planes, job.aabbs[i]) == OUTSIDE) continue;
		job.result[job.meshes[i]].model_m.append(job.model_m[i]);
	}

	cull_node(job.result, *job.partition, job.partition->nodes[0], 0, job.planes);
}

void cull_meshes(const ScenePartition& scene_partition, World& world, MeshBuckets& buckets, uint count, CulledMeshBucket** culled_mesh_bucket, Viewport viewports[], EntityQuery query) {
	tvector<AABB> aabbs;
	tvector<glm::mat4> model_m;
	tvector<int> meshes;
	
	assign_meshes_to_buckets(world, buckets, aabbs, model_m, meshes, query.with_none(STATIC));
	
	CullMeshJob job[10];
	JobDesc desc[10];
	assert(count <= 10);

	for (uint pass = 0; pass < count; pass++) {
		job[pass] = { &scene_partition, &buckets, aabbs, model_m, meshes, viewports[pass].frustum_planes, culled_mesh_bucket[pass] };
		desc[pass] = { cull_mesh_job, job + pass };
	}

	wait_for_jobs(PRIORITY_HIGH, { desc, count });
}

void render_node(RenderPass& ctx, material_handle mat, model_handle cube, ScenePartition& scene_partition, uint node_index) {
	Node& node = scene_partition.nodes[node_index];

	Transform trans;
	trans.position = (node.aabb.max + node.aabb.min) * 0.5f;
	trans.scale = node.aabb.max - node.aabb.min;
	trans.scale *= 0.5f;

	draw_mesh(ctx.cmd_buffer, cube, mat, trans);

	for (int i = 0; i < 2; i++) {
		if (node.child[i]) render_node(ctx, mat, cube, scene_partition, node.child[i]);
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
