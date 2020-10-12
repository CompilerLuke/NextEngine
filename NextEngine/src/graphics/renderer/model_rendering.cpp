#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include "components/transform.h"
#include "core/memory/linear_allocator.h"
#include "core/io/logger.h"
#include "graphics/rhi/draw.h"
#include "graphics/assets/assets.h"

#include "graphics/assets/material.h"

//todo IT MIGHT BE MORE EFFICIENT TO ALLOCATE THE INSTANCE BUFFER ON THE FLY
//INSTEAD OF PREALLOCATING, AS IT MEMORY CAN BE DISTRIBUTED MORE DYNAMICALLY

/*
void ModelRendererSystem::pre_render() { 	
	auto& renderer = gb::renderer;
	auto filtered = world.filter<ModelRenderer, Materials, Transform>(params.layermask);

	for (int i = 0; i < NUM_INSTANCES; i++) {
		instances[i].transforms.clear();
		instances[i].aabbs.clear();
		instances[i].ids.clear();
	}

	glm::mat4* model_m = renderer.model_m;

	for (unsigned int i = 0; i < filtered.length; i++) {
		ID id = filtered[i];

		auto self = world.by_id<ModelRenderer>(id);
		auto materials = world.by_id<Materials>(id);
		auto trans = world.by_id<Transform>(id);

		if (!self->visible) continue;
		if (self->model_id.id == INVALID_HANDLE) continue;

		auto model = RHI::model_manager.get(self->model_id);
		if (!model) continue;

		//if (cull(planes, model->aabb.apply(params.model_m[id]))) continue;

		assert(model->materials.length == materials->materials.length);

		for (Mesh& mesh : model->meshes) {
			Handle<Material> mat = materials->materials[mesh.material_id];

			unsigned int mat_index = mat.id - 1;

			auto& instance = instances[mesh.buffer.vao * MAX_VAO + mat_index];

			instance.buffer = &mesh.buffer;
			instance.ids.append(id);
			instance.aabbs.append(mesh.aabb.apply(model_m[id]));
			instance.transforms.append(model_m[id]);
		}
	}

	for (Instance& inst : instances) {
		if (inst.instance_buffer.buffer == 0 && inst.aabbs.length > 0) {
			inst.instance_buffer = SubInstanceBuffer(inst.v);
			new (&inst.instance_buffer) InstanceBuffer(*inst.buffer, INSTANCE_MAT4X4_LAYOUT, inst.transforms.data, inst.transforms.length);
		}
		else if (inst.aabbs.length > 0) {
			inst.instance_buffer.data(inst.transforms);
		}
	}
}*/

bool bit_set(uint* bit_visible, int i) {
	uint chunk_visible = bit_visible[i / 32];
	uint mask = chunk_visible & (1 << i % 32);
	return mask != 0;
}


void render_meshes(const MeshBucketCache& mesh_buckets, CulledMeshBucket* buckets, RenderPass& ctx) {
	//return; //todo handle support for z-prepass
	
	bool depth_only = ctx.type == RenderPass::Depth;
	CommandBuffer& cmd_buffer = ctx.cmd_buffer;

	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);

	for (uint i = 0; i < MAX_MESH_BUCKETS; i++) {
		const MeshBucket& bucket = mesh_buckets.keys[i];
		CulledMeshBucket& instances = buckets[i];
		int count = instances.model_m.length;

		if (!(bucket.flags & CAST_SHADOWS) && ctx.id != RenderPass::Scene) continue;
		if (count == 0) continue;

		//todo performance: this goes through three levels of indirection
		VertexBuffer vertex_buffer = get_vertex_buffer(bucket.model_id, bucket.mesh_id);
		InstanceBuffer instance_offset = frame_alloc_instance_buffer<glm::mat4>(INSTANCE_LAYOUT_MAT4X4, instances.model_m);

		bind_pipeline(cmd_buffer, depth_only ? bucket.depth_only_pipeline_id :  bucket.color_pipeline_id);
		
		if (!depth_only) bind_material(cmd_buffer, bucket.mat_id);
		
		draw_mesh(cmd_buffer, vertex_buffer, instance_offset);
	}
}