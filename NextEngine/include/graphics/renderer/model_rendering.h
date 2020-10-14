#pragma once

#include "engine/core.h"
#include "graphics/assets/model.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/rhi/buffer.h"
#include "graphics/pass/pass.h"
#include "core/container/hash_map.h"
#include "core/container/tvector.h"

using RenderFlags = uint;
constexpr RenderFlags CAST_SHADOWS = 1 << 0;

struct MeshBucket {
	material_handle mat_id;
	pipeline_handle depth_only_pipeline_id;
	pipeline_handle color_pipeline_id;
	model_handle model_id;
	uint mesh_id;
	uint flags;

	inline bool operator==(const MeshBucket& other) const {
		return mat_id.id == other.mat_id.id
			&& depth_only_pipeline_id.id == other.depth_only_pipeline_id.id
			&& color_pipeline_id.id == other.color_pipeline_id.id
			&& model_id.id == other.model_id.id
			&& mesh_id == other.mesh_id
			&& flags == other.flags;
	}
	
	inline bool operator!=(const MeshBucket& other) const {
		return !(*this == other);
	}
};

struct CulledMeshBucket {
	tvector<glm::mat4> model_m;
};

constexpr int MAX_MESH_BUCKETS = 103;

using MeshBucketCache = hash_set<MeshBucket, MAX_MESH_BUCKETS>;
void render_meshes(const MeshBucketCache& mesh_buckets, CulledMeshBucket* buckets, RenderPass& ctx);

inline u64 hash_func(MeshBucket& bucket) {
	return bucket.mat_id.id << 20 | bucket.model_id.id << 8 | bucket.mesh_id << 0;
}

