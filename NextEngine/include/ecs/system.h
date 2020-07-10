#pragma once

#include "id.h"
#include <glm/mat4x4.hpp>
#include "core/handle.h"
#include "core/core.h"
#include "graphics/culling/culling.h"

struct ENGINE_API UpdateCtx {
	Layermask layermask;
	struct Input& input;
	double delta_time = 0;

	UpdateCtx(struct Time&, struct Input&);
};

struct ENGINE_API System {
	virtual void update(struct World& world, struct UpdateCtx& ctx) = 0;
	virtual ~System() {}
};

void ENGINE_API register_default_systems_and_components(World& world);

#define DEFINE_COMPONENT_ID(type, id) \
template<> \
int constexpr type_id<struct type>() { return id; } \
template<> \
int constexpr type_id<const struct type>() { return id; }

#define DEFINE_APP_COMPONENT_ID(type, id) \
template<> \
int constexpr type_id<type>() { return 50 + id; }

//TODO MOVE ALL INTO ENGINE