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
	material_handle mat;
	pipeline_handle depth_prepass;
	pipeline_handle color_pipeline;
	pipeline_handle depth_only_pipeline;
	model_handle model;
	uint mesh_id;
	uint flags;

	inline bool operator==(const MeshBucket& other) const {
		return mat.id == other.mat.id
			&& depth_only_pipeline.id == other.depth_only_pipeline.id
			&& depth_prepass.id == other.depth_prepass.id
			&& color_pipeline.id == other.color_pipeline.id
			&& model.id == other.model.id
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
	return bucket.mat.id << 20 | bucket.model.id << 8 | bucket.mesh_id << 0;
}

