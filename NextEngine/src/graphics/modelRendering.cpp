#include "stdafx.h"
#include "graphics/modelRendering.h"
#include "graphics/renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include "components/transform.h"
#include "core/temporary.h"
#include "logger/logger.h"
#include "graphics/rhi.h"
#include "graphics/materialSystem.h"

REFLECT_STRUCT_BEGIN(ModelRenderer)
REFLECT_STRUCT_MEMBER(visible)
REFLECT_STRUCT_MEMBER_TAG(model_id)
REFLECT_STRUCT_END()

void ModelRendererSystem::pre_render(World& world, PreRenderParams& params) {
	auto& renderer = params.renderer;
	auto filtered = world.filter<ModelRenderer, Materials, Transform>(params.layermask);
	glm::mat4* model_m = renderer.model_m;

	for (int i = 0; i < NUM_INSTANCES; i++) {
		instances[i].transforms.clear();
		instances[i].aabbs.clear();
		instances[i].ids.clear();
	}

	for (unsigned int i = 0; i < filtered.length; i++) {
		ID id = filtered[i];

		auto self = world.by_id<ModelRenderer>(id);
		auto materials = world.by_id<Materials>(id);

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
}

void ModelRendererSystem::render(World& world, RenderParams& params) { //HOTSPOT

																	   //Once automatically instanced, draw the commands

																	   //todo fix bug

	glm::vec4 planes[6];
	extract_planes(params, planes);

	for (unsigned int i = 0; i < NUM_INSTANCES; i++) {
		unsigned int length = instances[i].transforms.length;
		if (length == 0) continue;

		unsigned int vao = i / MAX_VAO;
		unsigned int mat_index = i % MAX_VAO;

		Material* mat = RHI::material_manager.get({ mat_index + 1 });
		if (mat == NULL) continue;

		auto& instance = instances[i];

		auto shad = RHI::shader_manager.get(mat->shader);

		if (false) {
			DrawCommand cmd(instance.ids[0], &instance.transforms[0], instance.buffer, mat);
			cmd.num_instances = instance.transforms.length;
			params.command_buffer->submit(cmd);
		}
		else {
			for (int i = 0; i < instance.transforms.length; i++) {
				if (cull(planes, instance.aabbs[i])) continue;

				DrawCommand cmd(instance.ids[i], &instance.transforms[i], instance.buffer, mat);

				params.command_buffer->submit(cmd);
			}
		}
	}
}