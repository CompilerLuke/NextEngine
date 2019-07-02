#pragma once

#include "ecs/ecs.h"
#include <glm/mat4x4.hpp>
#include "culling.h"
#include "buffer.h"
#include <vector>
#include "core/temporary.h"
#include <unordered_map>
#include "ecs/ecs.h"
#include "reflection/reflection.h"

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

	REFLECT()
};

extern DrawCommandState default_draw_state;

#include "materialSystem.h"

using DrawSortKey = long long;

struct DrawCommand {
	DrawSortKey key = 0;
	ID id;
	glm::mat4* model_m;
	AABB* aabb;
	VertexBuffer* buffer;
	struct Material* material;

	DrawCommand(ID, glm::mat4*, AABB*, VertexBuffer*, Material*);
};

struct CommandBuffer {
	vector<DrawCommand> commands;
	std::unordered_map<DrawSortKey, int> instanced_buffers;

	unsigned int current_texture_index = 0;

	unsigned int next_texture_index();

	CommandBuffer();
	~CommandBuffer();

	void submit(DrawCommand&);
	void clear();

	void submit_to_gpu(World&, RenderParams&);
};