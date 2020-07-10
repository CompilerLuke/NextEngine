#pragma once

#include "core/core.h"
#include "core/container/tvector.h"
#include "graphics/assets/texture.h"
#include "graphics/rhi/buffer.h"

#define MAX_SET 3
#define MAX_DESCRIPTORS 10
#define MAX_UBOS 5
#define MAX_SAMPLERS 10
#define MAX_BINDING 20
#define MAX_FIELDS 5

enum Stage : char {
	VERTEX_STAGE = 1 << 0,
	FRAGMENT_STAGE = 1 << 1
};

inline Stage operator|(Stage a, Stage b) {
	return (Stage)((uint)a | (uint)b);
}

inline void operator|=(Stage& a, Stage b) {
	a = a | b;
}

struct CombinedSampler {
	sampler_handle sampler;
	texture_handle texture;
	cubemap_handle cubemap;
};

//todo no need to recreate combined samplers
//even many copies of the default sampler and texture, will be created
//each texture could have it's own cache

struct DescriptorDesc {
	enum Type { COMBINED_SAMPLER, UBO_BUFFER };

	struct Binding {
		uint binding;
		Stage stage;
		Type type;
		
		union {
			slice<CombinedSampler> samplers;
			slice<UBOBuffer> ubos;
		};

		Binding() {}
	};

	array<10, Binding> bindings;
};

ENGINE_API void add_combined_sampler(DescriptorDesc&, Stage, slice<CombinedSampler>, uint binding);
ENGINE_API void add_combined_sampler(DescriptorDesc&, Stage, sampler_handle, cubemap_handle, uint binding);
ENGINE_API void add_combined_sampler(DescriptorDesc&, Stage, sampler_handle, texture_handle, uint binding);
ENGINE_API void add_ubo(DescriptorDesc&, Stage, slice<UBOBuffer>, uint binding);

descriptor_set_handle alloc_descriptor_set();
ENGINE_API void update_descriptor_set(descriptor_set_handle&, DescriptorDesc&);