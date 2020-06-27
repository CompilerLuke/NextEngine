#pragma once

#include "scene_partition.h"
#include "graphics/renderer/model_rendering.h"
#include "aabb.h"
#include "graphics/pass/pass.h"

enum CullResult { INTERSECT, INSIDE, OUTSIDE };

void ENGINE_API extract_planes(const Viewport&, glm::vec4 planes[6]);
CullResult ENGINE_API frustum_test(glm::vec4 planes[6], const AABB& aabb);

struct World;
struct ModelRendererSystem;

ENGINE_API void build_acceleration_structure(ScenePartition& scene_partition, hash_set<MeshBucket, MAX_MESH_BUCKETS>& mesh_buckets, World& world);
ENGINE_API void update_acceleration_structure(ScenePartition& scene_partition, hash_set<MeshBucket, MAX_MESH_BUCKETS>& mesh_buckets, World& world);

void render_debug_bvh(ScenePartition& scene_partition, RenderPass&);

void cull_meshes(const ScenePartition& scene_partition, CulledMeshBucket* culled_mesh_bucket, const Viewport&);