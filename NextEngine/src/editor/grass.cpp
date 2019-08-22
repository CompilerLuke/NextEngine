#include "stdafx.h"
#include "editor/grass.h"
#include "editor/terrain.h"
#include "ecs/ecs.h"
#include "graphics/draw.h"
#include "graphics/materialSystem.h"
#include "graphics/culling.h"
#include "graphics/rhi.h"
#include <cstdlib>

REFLECT_STRUCT_BEGIN(Grass)
REFLECT_STRUCT_MEMBER(placement_model)
REFLECT_STRUCT_MEMBER(cast_shadows)
REFLECT_STRUCT_MEMBER(width)
REFLECT_STRUCT_MEMBER(height)
REFLECT_STRUCT_MEMBER(max_height)
REFLECT_STRUCT_MEMBER(density)
REFLECT_STRUCT_MEMBER(random_rotation)
REFLECT_STRUCT_MEMBER(random_scale)
REFLECT_STRUCT_MEMBER(align_to_terrain_normal)
REFLECT_STRUCT_MEMBER_TAG(transforms, reflect::HideInInspectorTag)
REFLECT_STRUCT_END()

void Grass::place(World& world) {
	auto terrains = world.filter<Terrain>(); //todo move this code into ecs.h
	if (terrains.length == 0) return;
	auto terrain = terrains[0];

	auto terrain_transform = world.by_id<Transform>(world.id_of(terrain));

	auto transform = world.by_id<Transform>(world.id_of(this));

	glm::vec2 step = (1.0f / density) / glm::vec2(this->width, this->height);
	this->transforms.clear();

	for (float a = -.5f * this->width; a < .5f * this->width; a += step.x) {
		for (float b = -.5f * this->height; b < .5f * this->height; b += step.x) {
			this->transforms.append({
				glm::vec3(a, 0, b),
				glm::angleAxis(glm::radians(random_rotation * (rand() % 360)), glm::vec3(0, 1.0f,0)),
				glm::vec3(1)
			});
		}
	}
}

void GrassSystem::render(World& world, RenderParams& params) {
	glm::vec4 planes[6];
	extract_planes(params, planes);
	
	for (ID id : world.filter<Grass, Transform, Materials>(params.layermask)) {
		Grass* grass = world.by_id<Grass>(id);
		Transform* trans = world.by_id<Transform>(id);
		Materials* materials = world.by_id<Materials>(id);

		if (!grass->cast_shadows && params.layermask & shadow_layer) continue;

		AABB aabb;
		aabb.min = trans->position - 0.5f * glm::vec3(grass->width, 0, grass->height);
		aabb.max = trans->position + 0.5f * glm::vec3(grass->width, 0, grass->height);

		if (cull(planes, aabb)) continue;

		glm::mat4* transforms = TEMPORARY_ARRAY(glm::mat4, grass->transforms.length);

		for (unsigned int i = 0; i < grass->transforms.length; i++) {
			Transform grass_trans = grass->transforms[i];
			grass_trans.position += trans->position;

			transforms[i] = grass_trans.compute_model_matrix();
		}

		Model* model = RHI::model_manager.get(grass->placement_model);
		Material* mat = RHI::material_manager.get(materials->materials[0]);

		if (model == NULL || mat == NULL) continue;

		DrawCommand cmd(id, transforms, &model->meshes[0].buffer, mat);
		cmd.num_instances = grass->transforms.length;
		params.command_buffer->submit(cmd);
	}
}