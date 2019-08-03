#pragma once

#include "id.h"
#include <glm/mat4x4.hpp>
#include "core/handle.h"
#include "core/core.h"

struct RenderParams {
	Layermask layermask;
	struct CommandBuffer* command_buffer;
	struct Pass* pass;
	struct Skybox* skybox;
	struct DirLight* dir_light;

	glm::mat4 projection;
	glm::mat4 view;
	struct Camera* cam = NULL;

	unsigned int width = 0;
	unsigned int height = 0;
	void set_shader_scene_params(Handle<struct Shader>, Handle<struct ShaderConfig>, struct World&);

	RenderParams(struct CommandBuffer*, struct Pass*);
};

struct UpdateParams {
	Layermask layermask;
	struct Input& input;
	double delta_time = 0;

	UpdateParams(struct Input&);
};

struct System {
	virtual void render(struct World& world, struct RenderParams& params) {};
	virtual void update(struct World& world, struct UpdateParams& params) {};
	virtual ~System() {}
};

void ENGINE_API register_default_systems_and_components(World& world);

#define DEFINE_COMPONENT_ID(type, id) \
template<> \
typeid_t constexpr ENGINE_API type_id<type>() { return id; }

#define DEFINE_GAME_COMPONENT_ID(type, id) \
template<> \
typeid_t constexpr type_id<type>() { return id; }

