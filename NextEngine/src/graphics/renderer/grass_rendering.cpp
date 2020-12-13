#include "graphics/renderer/grass.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include "components/grass.h"
#include "graphics/assets/material.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/assets.h"
#include "core/profiler.h"
#include "graphics/culling/culling.h"
#include "graphics/rhi/draw.h"

#include <algorithm>
#include "core/job_system/job.h"

glm::mat4* compute_model_matrices(vector<Transform>& transforms) {
	glm::mat4* result = TEMPORARY_ARRAY(glm::mat4, transforms.length);

	for (unsigned int i = 0; i < transforms.length; i++) {
		result[i] = compute_model_matrix(transforms[i]); 
	}

	return result;
}

struct CullGrassInput {
	glm::vec3 cam_pos;
	uint lod_bias;
	AABB model_aabb;
	float culling_distance;
	glm::vec4 planes[6];
	uint lod_count;
	array<MAX_MESH_LOD, float> lod_distance_sq;
};

struct CullGrassJob {
	CullGrassInput* input;
	slice<glm::vec3> positions;
	slice<glm::mat4> model_m;
	tvector<glm::mat4> output[MAX_MESH_LOD];
};

void cull_grass_particles(CullGrassJob& job) {
	CullGrassInput input = *job.input;
	LinearAllocator& allocator = get_thread_local_temporary_allocator();
	for (uint i = 0; i < MAX_MESH_LOD; i++) {
		job.output[i].allocator = &allocator;
	}

	for (uint i = 0; i < job.positions.length; i++) {
		glm::vec3 position = job.positions[i];

		glm::vec3 vec = position - input.cam_pos;
		float dist = (vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);

		if (dist > input.culling_distance) continue;

		AABB aabb = input.model_aabb.apply(job.model_m[i]);
		if (frustum_test(input.planes, aabb) == OUTSIDE) continue;

		//float grazing_multiplier = glm::abs(glm::dot(glm::normalize(position - cam_pos), glm::vec3(0,1,0)));
		//grazing_multiplier = 1.0 - grazing_multiplier;

		int lod = input.lod_count - 1;

		for (uint i = 0; i < input.lod_count; i++) {
			if (dist <= input.lod_distance_sq[i]) {
				lod = i;
				break;
			}
		}

		lod = glm::min(lod + input.lod_bias, input.lod_count - 1);

		job.output[lod].append(job.model_m[i]);
	}
}

void extract_grass_render_data(GrassRenderData& data, World& world, Viewport viewports[]) {
	Profile profile("Extract grass render data");

	LinearAllocator& temporary = get_temporary_allocator();
	
	for (auto [e, trans, grass, materials] : world.filter<Transform, Grass, Materials>()) {
		Model* model = get_Model(grass.placement_model);
		if (model == NULL) continue;

		InstanceBuffer instance_buffers[RenderPass::ScenePassCount][MAX_MESH_LOD] = {};
		CullGrassInput input[RenderPass::ScenePassCount];
		input[0].lod_count = model->lod_distance.length;
		input[0].lod_distance_sq = model->lod_distance;
		for (float& dist : input[0].lod_distance_sq) {
			dist *= dist;
		}

		input[0].culling_distance = input[0].lod_distance_sq.last();
		input[0].model_aabb = model->aabb;

		for (uint i = 1; i < RenderPass::ScenePassCount; i++) {
			input[i] = input[0];
		}

		slice<CullGrassJob> job_results[RenderPass::ScenePassCount];
		tvector<JobDesc> job_desc;

		for (uint pass = 0; pass < RenderPass::ScenePassCount; pass++) {
			bool shadow_pass = RenderPass::Shadow0 <= pass && pass <= RenderPass::Shadow3;
			if (!grass.cast_shadows && shadow_pass) {
				continue;
			}

			tvector<glm::mat4> model_m[MAX_MESH_LOD];

			input[pass].lod_bias = shadow_pass ? 1 : 0;
			input[pass].cam_pos = viewports[0].cam_pos;
			memcpy(input[pass].planes, viewports[pass].frustum_planes, sizeof(Viewport::frustum_planes));

			//glm::mat4 view_m = viewports[pass].view;
			//glm::vec2 horizontal_cam_pos(cam_pos.x, cam_pos.z);

			//glm::vec3 viewing_dir = glm::normalize(view_m * glm::vec4(0,0,1,1) - view_m * glm::vec4(0,0,0,1));
			//float looking_down = glm::abs(glm::dot(viewing_dir, glm::vec3(0,1,0)));

			uint bucket_size = 10000;
			uint buckets = (uint)ceilf(grass.positions.length / (float)bucket_size);

			CullGrassJob* jobs = (CullGrassJob*)temporary.allocate(sizeof(CullGrassJob) * buckets);

			for (uint i = 0; i < buckets; i++) {
				jobs[i] = {};
				jobs[i].input = input + pass;
				jobs[i].model_m.data = grass.model_m.data + (size_t)i * bucket_size;
				jobs[i].positions.data = grass.positions.data + (size_t)i * bucket_size;
				jobs[i].model_m.length = i + 1 == buckets ? grass.positions.length - i * bucket_size : bucket_size;
				jobs[i].positions.length = jobs[i].model_m.length;

				job_desc.append(JobDesc{ cull_grass_particles, jobs + i });
			}

			job_results[pass] = {jobs, buckets};
		}

		wait_for_jobs(PRIORITY_HIGH, job_desc);

		for (uint pass = 0; pass < RenderPass::ScenePassCount; pass++) {
			tvector<glm::mat4> model_m[MAX_MESH_LOD];

			for (CullGrassJob& job : job_results[pass]) { //merge output of jobs
				for (uint lod = 0; lod < input[pass].lod_count; lod++) {
					model_m[lod] += job.output[lod];
				}
			}

			//todo optimize
			for (int mesh_index = 0; mesh_index < model->meshes.length; mesh_index++) {
				Mesh& mesh = model->meshes[mesh_index];
				material_handle mat_handle = mat_by_index(materials, mesh.material_id);

				for (uint lod = 0; lod < input[pass].lod_count; lod++) {
					if (model_m[lod].length == 0) continue;

					VertexBuffer vertex_buffer = mesh.buffer[lod];

					GrassInstance instance = {};
					instance.vertex_buffer = vertex_buffer;
					instance.instances = model_m[lod];
					instance.depth_pipeline = query_pipeline(mat_handle, RenderPass::Shadow0, 0);
					instance.depth_prepass_pipeline = query_pipeline(mat_handle, RenderPass::Scene, 0);
					instance.color_pipeline = query_pipeline(mat_handle, RenderPass::Scene, 1);
					instance.material = mat_handle;

					if (model_m[lod].length > 0) data.instances[pass].append(instance);
					
				}
			}
		}
	}

	for (uint pass = 0; pass < 1; pass++) {
		std::sort(data.instances[pass].begin(), data.instances[pass].end(), [](GrassInstance& a, GrassInstance& b) {
			return a.color_pipeline.id < b.color_pipeline.id && a.material.id < b.material.id;
		});
	}
}

void render_grass(const GrassRenderData& data, RenderPass& render_pass) {
	CommandBuffer& cmd_buffer = render_pass.cmd_buffer;
	bool depth_only = render_pass.type == RenderPass::Depth;
	bool depth_prepass = render_pass.id == RenderPass::Scene && depth_only;

	tvector<GrassInstance> instances = data.instances[render_pass.id];
	
	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);

	for (GrassInstance& instance : instances) {
		bind_pipeline(cmd_buffer, depth_prepass ? instance.depth_prepass_pipeline : depth_only ? instance.depth_pipeline : instance.color_pipeline);
		bind_material(cmd_buffer, instance.material);

		//todo redundantly fills instance buffer for multiple meshes in model
		InstanceBuffer instance_buffer = frame_alloc_instance_buffer(INSTANCE_LAYOUT_MAT4X4, instance.instances);
		draw_mesh(cmd_buffer, instance.vertex_buffer, instance_buffer);
	}
}
