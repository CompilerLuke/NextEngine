#include "graphics/renderer/grass.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include "components/grass.h"
#include "graphics/assets/material.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/model.h"

glm::mat4* compute_model_matrices(vector<Transform>& transforms) {
	glm::mat4* result = TEMPORARY_ARRAY(glm::mat4, transforms.length);

	for (unsigned int i = 0; i < transforms.length; i++) {
		result[i] = compute_model_matrix(transforms[i]); 
	}

	return result;
}


/*
GrassRenderSystem::GrassRenderSystem(World& world) {
	world.on_make<Grass>([&world, this](vector<ID>& created) {
	for (ID id : created) {
	Grass* grass = world.by_id<Grass>(id);

	Model* model = RHI::model_manager.get(grass->placement_model);

	int length = grass->transforms.length;

	glm::mat4* transforms = compute_model_matrices(grass->transforms);

	GrassRenderObject grass_render_object;
	grass_render_object.instance_buffer = RHI::alloc_instance_buffer(VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4, length, transforms);


	grass->render_object = render_objects.make(std::move(grass_render_object));
	}
	});

	world.on_free<Grass>([&world, this](vector<ID>& destroyed) {
	for (ID id : destroyed) {
	Grass* grass = world.by_id<Grass>(id);
	render_objects.free(grass->render_object);
	}
	});
}
*/

/*
void GrassRenderSystem::update_buffers(World& world, ID id) {
	GrassRenderSystem* system = gb::renderer.grass_renderer;

	Grass* grass = world.by_id<Grass>(id);
	GrassRenderObject* render_object = system->render_objects.get(grass->render_object);

	glm::mat4* transforms = compute_model_matrices(grass->transforms);

	if (render_object->instance_buffer.capacity < grass->transforms.length * sizeof(glm::mat4)) {
	render_object->instance_buffer = RHI::alloc_instance_buffer(VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4, grass->transforms.length, transforms);
	}
	else {
	RHI::upload_data(render_object->instance_buffer, grass->transforms.length, transforms);
	}
}
*/

/*
void GrassRenderSystem::render(World& world, RenderPass& ctx) {
	/*
	glm::vec4 planes[6];
	extract_planes(ctx, planes);

	for (ID id : world.filter<Grass, Transform, Materials>(ctx.layermask)) {
	Grass* grass = world.by_id<Grass>(id);
	Transform* trans = world.by_id<Transform>(id);
	Materials* materials = world.by_id<Materials>(id);

	if (!grass->cast_shadows && ctx.layermask & SHADOW_LAYER) continue;

	AABB aabb;
	aabb.min = trans->position - 0.5f * glm::vec3(grass->width, 0, grass->height);
	aabb.max = trans->position + 0.5f * glm::vec3(grass->width, 0, grass->height);

	if (cull(planes, aabb)) continue;

	Model* model = RHI::model_manager.get(grass->placement_model);
	GrassRenderObject* render_object = render_objects.get(grass->render_object);

	for (Mesh& mesh : model->meshes) {
	Material* mat = RHI::material_manager.get(materials->materials[mesh.material_id]);

	DrawCommand cmd(id, grass->transforms.length, &mesh.buffer, &render_object->instance_buffer, mat);
	ctx.command_buffer->submit(cmd);
	}
	}
}
*/