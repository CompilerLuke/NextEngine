#include "stdafx.h"
#include "graphics/renderer/model_rendering.h"
#include "graphics/renderer/renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include "components/transform.h"
#include "core/memory/linear_allocator.h"
#include "core/io/logger.h"
#include "graphics/assets/asset_manager.h"

REFLECT_STRUCT_BEGIN(ModelRenderer)
REFLECT_STRUCT_MEMBER(visible)
REFLECT_STRUCT_MEMBER_TAG(model_id)
REFLECT_STRUCT_END()

ModelRendererSystem::ModelRendererSystem(AssetManager& asset_manager) : asset_manager(asset_manager) {
	for (int i = 0; i < Pass::Count; i++) {
		instance_buffer[i] = RHI::alloc_instance_buffer(VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4, MAX_MESH_INSTANCES, 0);
	}
}

void ModelRendererSystem::pre_render() { 	
	/*auto& renderer = gb::renderer;
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
	}*/
}

bool bit_set(uint* bit_visible, int i) {
	uint chunk_visible = bit_visible[i / 32];
	uint mask = chunk_visible & (1 << i % 32);
	return mask != 0;
}


void ModelRendererSystem::render(World& world, RenderCtx& ctx) {
	CulledMeshBucket* buckets = pass_culled_bucket[ctx.pass->id];

	glm::mat4* instance_data = TEMPORARY_ARRAY(glm::mat4, MAX_MESH_INSTANCES);

	//GENERATE DRAW CALLS
	uint instance_count = 0;
	InstanceBuffer& instance_buffer = this->instance_buffer[ctx.pass->id];

	for (uint i = 0; i < MAX_MESH_BUCKETS; i++) {
		MeshBucket& bucket = mesh_buckets.keys[i];
		CulledMeshBucket & instances = buckets[i];
		int count = instances.model_m.length;

		if (count == 0) continue;

		memcpy(instance_data + instance_count, instances.model_m.data, instances.model_m.length * sizeof(glm::mat4));

		Model* model = asset_manager.models.get(bucket.model_id);
		VertexBuffer* vertex_buffer = &model->meshes[bucket.mesh_id].buffer;

		InstanceBuffer* instance_offset = TEMPORARY_ALLOC(InstanceBuffer);
		*instance_offset = instance_buffer;
		instance_offset->base += instance_count;
		instance_offset->size = count * sizeof(glm::mat4);

		Material* material = RHI::material_manager.get(bucket.mat_id);

		/*for (int j = 0; j < count; j++) {
			instance_data[j + instance_count] = instances.model_m[j];
		
			DrawCommand cmd(0, instances.model_m[j], vertex_buffer, material);
			ctx.command_buffer->submit(cmd);
		}*/

		ctx.command_buffer.draw(count, vertex_buffer, instance_offset, material);

		instance_count += count;
	}

	RHI::upload_data(instance_buffer, instance_count, instance_data);

	//flush_logger();
}