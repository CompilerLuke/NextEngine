#include "picking.h"
#include "editor.h"
#include "core/io/input.h"
#include "graphics/rhi/draw.h"
#include "core/memory/linear_allocator.h"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "graphics/assets/shader.h"
#include "core/memory/linear_allocator.h"
#include "core/io/logger.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "engine/engine.h"
#include "components/camera.h"
#include "components/terrain.h"
#include "graphics/renderer/renderer.h"
#include "core/container/tvector.h"
#include "graphics/culling/build_bvh.h"
#include "core/profiler.h"

#define MAX_ENTITIES_PER_NODE 10
#define MAX_PICKING_DEPTH 6

uint subdivide_BVH(PickingScenePartition& scene_partition, AABB& node_aabb, int depth, int mesh_count, AABB* aabbs, ID* ids, uint* tags) {
	uint node_id = scene_partition.node_count;
	
	if (mesh_count <= MAX_ENTITIES_PER_NODE || depth >= MAX_PICKING_DEPTH) {
		Node& node = alloc_leaf_node(scene_partition, node_aabb, MAX_PICKING_INSTANCES, mesh_count);
		
		copy_into_node(node, scene_partition.ids, ids);
		copy_into_node(node, scene_partition.aabbs, aabbs);
	}
	else {
		BranchNodeInfo info = alloc_branch_node(scene_partition, node_aabb);
		tvector<AABB> subdivided_aabbs[2];
		tvector<ID> subdivided_ids[2];
		tvector<uint> subdivided_tags[2];

		for (int i = 0; i < mesh_count; i++) {
			AABB& aabb = aabbs[i];

			if (bigger_than_leaf(info, aabb)) {
				int offset = elem_offset(scene_partition, info, aabb);
				scene_partition.aabbs[offset] = aabb;
				scene_partition.ids[offset] = ids[i];
				scene_partition.tags[offset] = tags[i];
			}
			else {
				bool node_index = split_index(info, aabb);
				subdivided_aabbs[node_index].append(aabb);
				subdivided_ids[node_index].append(ids[i]);
				subdivided_tags[node_index].append(tags[i]);
			}
		}

		for (int i = 0; i < 2; i++) {
			if (subdivided_aabbs[i].length == 0) continue;
			info.node.child[info.node.child_count++] = subdivide_BVH(scene_partition, info.child_aabbs[i], depth + 1, subdivided_aabbs[i].length, subdivided_aabbs[i].data, subdivided_ids[i].data, subdivided_tags[i].data);
			info.node.aabb.update_aabb(scene_partition.nodes[info.node.child[i]].aabb);
		}

		bump_allocator(scene_partition, info);
	}

	return node_id;
}

void build_acceleration_structure(PickingScenePartition& scene_partition, World& world) { 
	Profile profile("rebuild picking acceleration");
	
	AABB world_bounds;

	int occupied = temporary_allocator.occupied;

	tvector<AABB> aabbs;
	tvector<ID> ids;
	tvector<uint> tags;

	//todo only generate bvh for static elements
	for (auto [e, trans, model_renderer] : world.filter<Transform, ModelRenderer>()) {
		auto model_m = compute_model_matrix(trans);

		Model* model = get_Model(model_renderer.model_id);		
		if (model == NULL) continue;

		AABB aabb = model->aabb.apply(model_m);
		world_bounds.update_aabb(aabb);
		aabbs.append(aabb);
		ids.append(e.id);
		tags.append(0);
	}

	for (auto[e, trans, control] : world.filter<Transform, TerrainControlPoint>()) {
		AABB aabb;
		aabb.min = trans.position + glm::vec3(-0.5, -0.5, -0.5);
		aabb.max = trans.position + glm::vec3(0.5, 0.5, 0.5);

		world_bounds.update_aabb(aabb);
		aabbs.append(aabb);
		ids.append(e.id);
		tags.append(GIZMO_TAG);
	}

	for (auto[e, trans, control] : world.filter<Transform, TerrainSplat>()) {
		AABB aabb;
		aabb.min = trans.position + glm::vec3(-0.5, -0.5, -0.5);
		aabb.max = trans.position + glm::vec3(0.5, 0.5, 0.5);

		world_bounds.update_aabb(aabb);
		aabbs.append(aabb);
		ids.append(e.id);
		tags.append(GIZMO_TAG);
	}
	for (auto[e, trans, terrain] : world.filter<Transform, Terrain>()) {
		auto model_m = compute_model_matrix(trans);

		uint width = terrain.width;
		uint height = terrain.height;

		AABB terrain_aabb;
		terrain_aabb.min = trans.position;
		terrain_aabb.max = trans.position + glm::vec3(width * terrain.size_of_block, terrain.max_height, height * terrain.size_of_block);

		float* displacement_map = terrain.displacement_map[0].data;

		uint width_quads = width * 32;

		for (uint block_y = 0; block_y < height; block_y++) {
			for (uint block_x = 0; block_x < width; block_x++) {
				float heighest = FLT_MAX; //todo add support for ray traced heightmap

				uint offset_y = block_y * 32;
				uint offset_x = block_x * 32;

				for (uint y1 = offset_y; y1 < offset_y + 32; y1 += 4) {
					uint offset = y1 * width_quads;

					for (uint x1 = offset_x; x1 < offset_x + 32; x1 += 4) {
						heighest = fminf(heighest, displacement_map[x1 + offset]);
					}
				}

				AABB aabb;
				aabb.min = terrain_aabb.min + glm::vec3(block_x * terrain.size_of_block, 0, block_y * terrain.size_of_block);
				aabb.max = aabb.min + glm::vec3(terrain.size_of_block, heighest, terrain.size_of_block);


				world_bounds.update_aabb(aabb);
				aabbs.append(aabb);
				ids.append(e.id);
				tags.append(0);


			}
		}
	}

	subdivide_BVH(scene_partition, world_bounds, 0, aabbs.length, aabbs.data, ids.data, tags.data);

	temporary_allocator.occupied = occupied;
}

Ray ray_from_mouse(Viewport& viewport, glm::vec2 mouse_position) {
	glm::mat4 inv_proj_view = glm::inverse(viewport.proj * viewport.view);

	glm::vec2 mouse_position_clip = mouse_position / glm::vec2(viewport.width, viewport.height);
	mouse_position_clip.y = 1.0 - mouse_position_clip.y;
	mouse_position_clip = mouse_position_clip * 2.0f - 1.0f;

	glm::vec4 end = inv_proj_view * glm::vec4(mouse_position_clip, 1, 1.0);
	
#ifdef GLM_FORCE_DEPTH_ZERO_TO_ONE
	glm::vec4 start = inv_proj_view * glm::vec4(mouse_position_clip, 0.0f, 1.0);
#else 
	glm::vec4 start = inv_proj_view * glm::vec4(mouse_position_clip, -1, 1.0);
#endif

	start /= start.w;
	end /= end.w;

	return Ray(start, glm::normalize(end - start), glm::length(end - start));
}

float max(glm::vec3 vec) {
	return vec.x > vec.y ? (vec.x > vec.z ? vec.x : vec.z) : (vec.y > vec.z ? vec.y : vec.z);
}

//Math from - https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
float intersect(const AABB& bounds, const Ray &r) {
	float tmin, tmax, tymin, tymax, tzmin, tzmax;

	tmin = (bounds[r.sign[0]].x - r.orig.x) * r.invdir.x;
	tmax = (bounds[1 - r.sign[0]].x - r.orig.x) * r.invdir.x;
	tymin = (bounds[r.sign[1]].y - r.orig.y) * r.invdir.y;
	tymax = (bounds[1 - r.sign[1]].y - r.orig.y) * r.invdir.y;

	if ((tmin > tymax) || (tymin > tmax))
		return FLT_MAX;
	if (tymin > tmin)
		tmin = tymin;
	if (tymax < tmax)
		tmax = tymax;

	tzmin = (bounds[r.sign[2]].z - r.orig.z) * r.invdir.z;
	tzmax = (bounds[1 - r.sign[2]].z - r.orig.z) * r.invdir.z;

	if ((tmin > tzmax) || (tzmin > tmax))
		return FLT_MAX;
	if (tzmin > tmin)
		tmin = tzmin;
	if (tzmax < tmax)
		tmax = tzmax;

	return tmin;
}

void ray_cast_node(PickingScenePartition& partition, Node& node, const Ray& ray, RayHit& hit) {
	for (int i = 0; i < node.child_count; i++) { //could use count instead
		Node& child = partition.nodes[node.child[i]];
		if (intersect(child.aabb, ray) < ray.t) ray_cast_node(partition, child, ray, hit);
	}

	for (int i = node.offset; i < node.offset + node.count; i++) {
		AABB& aabb = partition.aabbs[i];

		glm::vec3 new_hit;
		float t = intersect(aabb, ray);
		bool always_visible = partition.tags[i] & GIZMO_TAG;

		if (0 < t && t < hit.t && t < ray.t) {
			hit.id = partition.ids[i];
			hit.t = t;
		}
	}
}

PickingSystem::PickingSystem() {

}

void PickingSystem::rebuild_acceleration_structure(World& world) {
	build_acceleration_structure(partition, world);
}

bool PickingSystem::ray_cast(const Ray& ray, RayHit& hit_result) {
	if (partition.count == 0) return false;
	ray_cast_node(partition, partition.nodes[0], ray, hit_result);

	bool hit = hit_result.t < FLT_MAX;
	if (hit) hit_result.position = ray.orig + ray.dir * hit_result.t;

	return hit;
}

bool PickingSystem::ray_cast(Viewport& viewport, glm::vec2 position, RayHit& hit) {
	Ray ray = ray_from_mouse(viewport, position);
	return ray_cast(ray, hit);
}

int PickingSystem::pick(Viewport& viewport, glm::vec2 position) {
	Ray ray = ray_from_mouse(viewport, position);
	RayHit hit;

	if (ray_cast(ray, hit)) return hit.id;
	else return -1;
}

void render_cube(RenderPass& ctx, material_handle mat_handle, glm::vec3 center, glm::vec3 scale) {
	Transform trans;
	trans.position = center;
	trans.scale = scale * 0.5f;

	draw_mesh(ctx.cmd_buffer, primitives.cube, mat_handle, trans);
}

struct Material;

void render_node(RenderPass& ctx, material_handle mat, PickingScenePartition& partition, Node& node, Ray& ray) {
	render_cube(ctx, mat, (node.aabb.max + node.aabb.min) * 0.5f, node.aabb.max - node.aabb.min);

	for (int i = 0; i < node.child_count; i++) {
		Node& child = partition.nodes[node.child[i]];
		if (intersect(child.aabb, ray) < FLT_MAX) render_node(ctx, mat, partition, child, ray);
	}

	for (int i = node.offset; i < node.offset + node.count; i++) {
		AABB& aabb = partition.aabbs[i];
		if (intersect(aabb, ray) < FLT_MAX) render_cube(ctx, mat, (aabb.max + aabb.min) * 0.5f, aabb.max - aabb.min);
	}
}


#include "graphics/assets/material.h"


void PickingSystem::visualize(Viewport& viewport, glm::vec2 position, RenderPass& ctx) {
	//return;
	if (partition.count == 0) return;
	
	Ray ray = ray_from_mouse(viewport, position);

	static material_handle material = {};

	if (material.id == INVALID_HANDLE) {
		MaterialDesc mat_desc{ load_Shader("shaders/pbr.vert", "shaders/gizmo.frag") };
		mat_vec3(mat_desc, "color", glm::vec3(1.0f, 0.0f, 0.0f));
		mat_desc.draw_state = Cull_None | PolyMode_Wireframe | (5 << WireframeLineWidth_Offset);

		material = make_Material(mat_desc);
	}

	//return;

	RayHit hit_result;
	ray_cast(ray, hit_result);

	render_cube(ctx, material, hit_result.position, glm::vec3(0.1));

	render_node(ctx, material, partition, partition.nodes[0], ray);

	//ray.dir = glm::vec3(0, 0, -1) * world.by_id<Transform>(get_camera(world, EDITOR_LAYER))->rotation;

	//render_cube(ctx, mat, models.get(cube), ray.orig + ray.dir * 5.0f, glm::vec3(0.1f));
	//render_cube(ctx, mat, models.get(cube), ray.orig + ray.dir * 10.0f, glm::vec3(0.1f));
	//render_cube(ctx, mat, models.get(cube), ray.orig + ray.dir * 20.0f, glm::vec3(0.1f));
}

void make_render_outline_state(OutlineRenderState& self) {
	self.outline_shader = load_Shader("shaders/outline.vert", "shaders/outline.frag");

	DrawCommandState object_state = StencilFunc_Always | (0xFF << StencilMask_Offset) | ColorMask_None | DepthFunc_None;

	int outline_width = 3;

	MaterialDesc outline_material{ self.outline_shader };
	outline_material.draw_state = StencilFunc_NotEqual | (0x00 << StencilMask_Offset) | DepthFunc_None | PolyMode_Wireframe | (outline_width << WireframeLineWidth_Offset);

	self.outline_material = make_Material(outline_material);
}

void render_object_selected_outline(OutlineRenderState& outline, World& world, slice<ID> highlighted, RenderPass& ctx) {
	for (ID id : highlighted) {
		auto has_components = world.get_by_id<Transform, ModelRenderer, Materials>(id);

		if (has_components) {
			auto[trans, model_renderer, materials] = *has_components;
			glm::mat4 model_m = compute_model_matrix(trans);

			draw_mesh(ctx.cmd_buffer, model_renderer.model_id, outline.object_material, model_m);
			draw_mesh(ctx.cmd_buffer, model_renderer.model_id, outline.outline_material, model_m);

			//Model* model = get_Model(model_renderer.model_id);

			/*for (Mesh& mesh : model->meshes) {
				material_handle should_be_handle = materials.materials[mesh.material_id];
				MaterialPipelineInfo info = material_pipeline_info(should_be_handle);

				

				//Material* mat = TEMPORARY_ALLOC(Material);
				//mat->params.allocator = &temporary_allocator;
				//*mat = *should_be;

				//mat->state = &object_state;
					
				//ctx.command_buffer.draw(model_m, &mesh.buffer, mat);
				//ctx.command_buffer.draw(model_m, &mesh.buffer, TEMPORARY_ALLOC(Material, outline_material)); //this might be leaking memory!
				//*/
			//}
		}
	}
}

