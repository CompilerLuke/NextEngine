#pragma once

#include "ecs/id.h"
#include "scene_partition.h"
#include "graphics/renderer/model_rendering.h"
#include "core/math/aabb.h"
#include "graphics/pass/pass.h"

enum CullResult { INTERSECT, INSIDE, OUTSIDE };

void ENGINE_API extract_planes(Viewport&);
CullResult ENGINE_API frustum_test(const glm::vec4 planes[6], const AABB& aabb);

struct World;
struct ModelRendererSystem;
struct Viewport;

ENGINE_API void build_acceleration_structure(ScenePartition& scene_partition, hash_set<MeshBucket, MAX_MESH_BUCKETS>& mesh_buckets, World& world);
ENGINE_API void update_acceleration_structure(ScenePartition& scene_partition, hash_set<MeshBucket, MAX_MESH_BUCKETS>& mesh_buckets, World& world);

void render_debug_bvh(ScenePartition& scene_partition, RenderPass&);

using MeshBuckets = hash_set<MeshBucket, MAX_MESH_BUCKETS>;

void cull_meshes(const ScenePartition& scene_partition, World& world, MeshBuckets& buckets, uint viewport_count, CulledMeshBucket** culled_mesh_bucket, Viewport viewports[], EntityQuery query);
