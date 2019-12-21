#pragma once

#include "ecs/id.h"
#include <glm/mat4x4.hpp>
#include <unordered_map>
#include "reflection/reflection.h"
#include "core/core.h"

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

using DrawSortKey = long long;

struct DrawCommand {
	static constexpr unsigned int update_instances = (1 << 0);

	ID id;
	glm::mat4* model_m;
	struct VertexBuffer* buffer;
	struct Material* material;
	int num_instances = 1;

	unsigned int flags = 0;

	DrawCommand(ID, glm::mat4*, struct VertexBuffer*, struct Material*);
};

struct Pipeline {
	Handle<struct Shader> shader;
	Handle<struct ShaderConfig> config;

	Handle<struct Uniform> model_u;
	Handle<struct Uniform> projection_u;
	Handle<struct Uniform> view_u;

	DrawCommandState state;
};

struct ENGINE_API CommandBuffer {
	vector<DrawCommand> commands;

	unsigned int current_texture_index = 0;

	unsigned int next_texture_index();

	CommandBuffer();
	~CommandBuffer();

	void submit(DrawCommand&);
	void clear();

	void submit_to_gpu(World&, RenderParams&);
};