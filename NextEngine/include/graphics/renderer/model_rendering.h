#pragma once

#include "graphics/assets/model.h"
#include "graphics/renderer/render_feature.h"
#include "graphics/rhi/buffer.h"
#include "graphics/pass/pass.h"
#include "core/container/hash_map.h"

using RenderFlags = uint;
constexpr RenderFlags CAST_SHADOWS = 1 << 0;

struct MeshBucket {
	material_handle mat_id;
	model_handle model_id;
	uint mesh_id;
	uint flags;

	inline bool operator==(const MeshBucket& other) {
		return mat_id.id == other.mat_id.id && model_id.id == other.model_id.id && mesh_id == other.mesh_id && flags == other.flags;
	}
};

inline uint hash_func(MeshBucket& bucket) {
	return bucket.mat_id.id << 20 | bucket.model_id.id << 8 | bucket.mesh_id << 0;
}

struct CulledMeshBucket {
	vector<glm::mat4> model_m;
};

constexpr int MAX_MESH_BUCKETS = 103;

struct ENGINE_API ModelRendererSystem : RenderFeature {
	struct AssetManager& asset_manager;
	struct BufferManager& buffer_manager;
	
	InstanceBuffer instance_buffer[Pass::Count];
	hash_set<MeshBucket, MAX_MESH_BUCKETS> mesh_buckets;
	CulledMeshBucket pass_culled_bucket[Pass::Count][MAX_MESH_BUCKETS];

	ModelRendererSystem(struct AssetManager&, struct BufferManager&);
	void pre_render();
	void render(World&, RenderCtx&) override;
};

