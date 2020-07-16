#pragma once

#include "id.h"

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

void ENGINE_API register_default_components(World& world);

template<typename T>
constexpr int type_id();

#define DEFINE_COMPONENT_ID(type, id) \
template<> \
inline int type_id<struct type>() { return id; } \
template<> \
inline int type_id<const struct type>() { return id; }

#define DEFINE_APP_COMPONENT_ID(type, id) \
template<> \
inline int type_id<type>() { return 50 + id; }

//TODO MOVE ALL INTO ENGINE