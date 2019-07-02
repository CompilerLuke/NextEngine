#include "stdafx.h"
#include "model/model.h"
#include "components/transform.h"
#include "core/temporary.h"
#include "graphics/rhi.h"

REFLECT_STRUCT_BEGIN(ModelRenderer)
REFLECT_STRUCT_MEMBER(visible)
REFLECT_STRUCT_MEMBER_TAG(model_id, reflect::ModelIDTag)
REFLECT_STRUCT_END()

void ModelRendererSystem::render(World& world, RenderParams& params) {
	for (ID id : world.filter<ModelRenderer, Materials, Transform>(params.layermask)) {
		auto transform = world.by_id<Transform>(id);
		auto self = world.by_id<ModelRenderer>(id);
		auto materials = world.by_id<Materials>(id);

		if (!self->visible) continue;
		if (self->model_id.id == INVALID_HANDLE) continue;

		auto model = RHI::model_manager.get(self->model_id);
		if (!model) continue;

		assert(model->materials.length == materials->materials.length);

		auto model_m = TEMPORARY_ALLOC(glm::mat4);
		*model_m = transform->compute_model_matrix();

		model->render(world.id_of(self), model_m, materials->materials, params);
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

