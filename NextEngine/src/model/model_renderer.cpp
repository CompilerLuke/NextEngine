#include "stdafx.h"
#include "model/model.h"
#include <glm/gtc/matrix_transform.hpp>
#include "components/transform.h"
#include "core/temporary.h"
#include "graphics/rhi.h"
#include "logger/logger.h"
#include <chrono>
#include <iostream>

REFLECT_STRUCT_BEGIN(ModelRenderer)
REFLECT_STRUCT_MEMBER(visible)
REFLECT_STRUCT_MEMBER_TAG(model_id, reflect::ModelIDTag)
REFLECT_STRUCT_END()

struct Instance {
	vector<glm::mat4> transforms;
	vector<AABB> aabbs;
	ID id = 0;
	VertexBuffer* buffer;
};

//#define PROFILING

#ifdef PROFILING
#define PROFILE_BEGIN()  { auto start = std::chrono::high_resolution_clock::now();
#define PROFILE_END(name) auto end = std::chrono::high_resolution_clock::now(); \
std::chrono::duration<double, std::milli> delta = end - start; \
std::cout << name << delta.count() << "ms" << std::endl; }
#endif 
#ifndef PROFILING
#define PROFILE_BEGIN()
#define PROFILE_END()
#endif

vector<Instance> instances; //todo move into system, 
//one possibility is splitting dynamic and static meshes

void ModelRendererSystem::render(World& world, RenderParams& params) { //HOTSPOT
	auto filtered = world.filter<ModelRenderer, Materials, Transform>(params.layermask);

	constexpr unsigned max_vao = 30;
	unsigned int num_instances = max_vao * RHI::material_manager.slots.length;

	if (instances.length < num_instances) {
		instances.reserve(num_instances);
		int diff = num_instances - instances.length;
		for (int i = 0; i < diff; i++) {
			instances.append(Instance());
		}
	}

	for (int i = 0; i < num_instances; i++) {
		instances[i].transforms.clear();
	}

	glm::mat4 identity(1.0);

	for (unsigned int i = 0; i < filtered.length; i++) {
		ID id = filtered[i];

		auto self = world.by_id<ModelRenderer>(id);
		auto materials = world.by_id<Materials>(id);

		if (!self->visible) continue;
		if (self->model_id.id == INVALID_HANDLE) continue;

		auto model = RHI::model_manager.get(self->model_id);
		if (!model) continue;

		assert(model->materials.length == materials->materials.length);


		auto static_trans = world.by_id<StaticTransform>(id);

		for (Mesh& mesh : model->meshes) {
			Handle<Material> mat = materials->materials[mesh.material_id];

			unsigned int mat_index = RHI::material_manager.get_index(mat);
			
			auto& instance = instances[mesh.buffer.vao + max_vao * mat_index];
			auto trans = world.by_id<Transform>(filtered[i]);

			if (static_trans) {
				instance.transforms.append(static_trans->model_matrix);
			}
			else {
				glm::mat4 translate = glm::translate(identity, trans->position);
				glm::mat4 scale = glm::scale(translate, trans->scale);
				glm::mat4 rotation = glm::mat4_cast(trans->rotation);

				instance.transforms.append(scale * rotation);
			}
			instance.buffer = &mesh.buffer;
			instance.id = id; //make ids an array
		}
	}

	
	for (unsigned int i = 0; i < num_instances; i++) {
		unsigned int length = instances[i].transforms.length;
		if (length == 0) continue;
		
		unsigned int vao = i % max_vao;
		unsigned int mat_index = i / max_vao;

		Material* mat = RHI::material_manager.get(RHI::material_manager.index_to_handle(mat_index));

		auto& instance = instances[i];

		auto transforms = instance.transforms.data;
		auto aabbs = instance.aabbs.data;

		DrawCommand cmd(instance.id, transforms, aabbs, instance.buffer, mat);
		cmd.num_instances = instance.transforms.length;
		params.command_buffer->submit(cmd);
	}
}

void ModelRenderer::set_materials(World& world, vector<Handle<Material>>& materials) {
	vector<Handle<Material>> materials_in_order;

	if (this->model_id.id == INVALID_HANDLE) return;
	
	auto model = RHI::model_manager.get(this->model_id);
	if (!model) return;

	for (auto& mat_name : model->materials) {
		auto mat = material_by_name(materials, mat_name);
		if (mat.id == INVALID_HANDLE) throw "Missing material";

		materials_in_order.append(mat);
	}

	auto this_id = world.id_of(this);
	auto material_component = world.by_id<Materials>(this_id);
	
	if (material_component == NULL) {
		material_component = world.make<Materials>(this_id);
	}
	material_component->materials = materials_in_order;
}

