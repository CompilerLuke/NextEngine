#include "graphics/renderer/grass.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include "components/grass.h"
#include "graphics/assets/material.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/assets.h"
#include "core/profiler.h"
#include "graphics/culling/culling.h"

#include <algorithm>

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


void extract_grass_render_data(GrassRenderData& data, World& world, Viewport viewports[RenderPass::ScenePassCount]) {
	Profile profile("Extract grass render data");

	glm::vec4 planes[6];
	extract_planes(viewports[0], planes);
	
	for (auto [e, trans, grass, materials] : world.filter<Transform, Grass, Materials>()) {
		Model* model = get_Model(grass.placement_model);
		if (model == NULL) continue;

		InstanceBuffer instance_buffers[RenderPass::ScenePassCount][MAX_MESH_LOD] = {};
		uint lod_count = model->lod_distance.length;

		auto lod_distance_sq = model->lod_distance;
		for (float& dist : lod_distance_sq) {
			dist *= dist;
		}

		float culling_distance = lod_distance_sq.last();
		AABB model_aabb = model->aabb;

		for (uint pass = 0; pass < 1; pass++) {
			tvector<glm::mat4> model_m[MAX_MESH_LOD];
			glm::mat4 view_m = viewports[pass].view;
			glm::vec3 cam_pos = viewports[pass].cam_pos;
			glm::vec2 horizontal_cam_pos(cam_pos.x, cam_pos.z);

			//glm::vec3 viewing_dir = glm::normalize(view_m * glm::vec4(0,0,1,1) - view_m * glm::vec4(0,0,0,1));
			//float looking_down = glm::abs(glm::dot(viewing_dir, glm::vec3(0,1,0)));

			for (uint i = 0; i < grass.positions.length; i++) {
				glm::vec3 position = grass.positions[i];

				glm::vec3 vec = position - cam_pos;
				float dist = (vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);

				if (dist > culling_distance) continue;

				AABB aabb = model_aabb.apply(grass.model_m[i]);
				if (frustum_test(planes, aabb) == OUTSIDE) continue;
				
				//float grazing_multiplier = glm::abs(glm::dot(glm::normalize(position - cam_pos), glm::vec3(0,1,0)));
				//grazing_multiplier = 1.0 - grazing_multiplier;

				int lod = lod_count - 1;
				
				for (uint i = 0; i < lod_count; i++) {
					if (dist <= lod_distance_sq[i]) {
						lod = i;
						break;
					}
				}

				model_m[lod].append(grass.model_m[i]);
			}

			for (int mesh_index = 0; mesh_index < model->meshes.length; mesh_index++) {
				Mesh& mesh = model->meshes[mesh_index];
				material_handle mat_handle = materials.materials[mesh.material_id]; //todo this is somewhat pointless indirection

				for (uint lod = 0; lod < lod_count; lod++) {
					if (model_m[lod].length == 0) continue;

					VertexBuffer vertex_buffer = mesh.buffer[lod];

					GrassInstance instance = {};
					instance.vertex_buffer = vertex_buffer;
					instance.instances = model_m[lod];
					instance.depth_pipeline = query_pipeline(mat_handle, RenderPass::Scene, 0);
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
	tvector<GrassInstance> instances = data.instances[render_pass.id];
	
	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);

	for (GrassInstance& instance : instances) {
		bind_pipeline(cmd_buffer, depth_only ? instance.depth_pipeline : instance.color_pipeline);
		bind_material(cmd_buffer, instance.material);

		//todo redundantly fills instance buffer for multiple meshes in model
		InstanceBuffer instance_buffer = frame_alloc_instance_buffer(INSTANCE_LAYOUT_MAT4X4, instance.instances);
		draw_mesh(cmd_buffer, instance.vertex_buffer, instance_buffer);
	}
}

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