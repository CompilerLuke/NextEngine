#pragma once

#include "ecs/id.h"
#include <glm/mat4x4.hpp>
#include "core/reflection.h"
#include "core/core.h"
#include "core/container/slice.h"

enum Cull {Cull_Front, Cull_Back, Cull_None};
enum DepthFunc {DepthFunc_Less, DepthFunc_Lequal, DepthFunc_None};
enum DrawOrder {
	draw_opaque = 0,
	draw_skybox = 1,
	draw_transparent = 2,
	draw_over = 3
};

enum StencilOp { Stencil_Keep_Replace };
enum StencilFunc { StencilFunc_Equal, StencilFunc_NotEqual, StencilFunc_Always, StencilFunc_None };
using StencilMask = unsigned int;
enum DrawMode { DrawSolid, DrawWireframe };

struct DrawCommandState {
	Cull cull = Cull_None;
	DepthFunc depth_func = DepthFunc_Lequal;
	bool clear_depth_buffer = false;
	DrawOrder order = draw_opaque;
	bool clear_stencil_buffer = false;
	StencilOp stencil_op = Stencil_Keep_Replace;
	StencilFunc stencil_func = StencilFunc_None;
	StencilMask stencil_mask = 0;
	DrawMode mode = DrawSolid;

	bool operator==(DrawCommandState&);

	REFLECT(ENGINE_API)
};

extern DrawCommandState ENGINE_API default_draw_state;
extern DrawCommandState ENGINE_API draw_draw_over;

using DrawSortKey = unsigned long long;

struct DrawCommand {
	glm::mat4 model_m;
	struct VertexBuffer* buffer;
	struct InstanceBuffer* instance_buffer = NULL;
	struct Material* material;
	int num_instances = 1;
};

struct Pipeline {
	shader_handle shader;
	shader_config_handle config;

	uniform_handle model_u;
	uniform_handle projection_u;
	uniform_handle view_u;

	DrawCommandState state;
};

struct VertexBuffer;
struct InstanceBuffer;
struct Material;
struct AssetManager;

struct ENGINE_API CommandBuffer {
	AssetManager& asset_manager;

	vector<DrawCommand> commands;

	unsigned int current_texture_index = 0;

	unsigned int next_texture_index();

	CommandBuffer(AssetManager& asset_manager);
	~CommandBuffer();

	void draw(glm::mat4, VertexBuffer*, Material*);
	void draw(glm::mat4, model_handle, slice<material_handle>);
	void draw(int length, model_handle, InstanceBuffer*, slice<material_handle>);
	void draw(int length, VertexBuffer*, InstanceBuffer*, Material*);
	void clear();

	static void submit_to_gpu(RenderCtx&);
};