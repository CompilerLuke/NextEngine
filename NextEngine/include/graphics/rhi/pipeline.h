#pragma once

#include "engine/handle.h"
#include "buffer.h"
#include "draw.h"
#include "graphics/pass/pass.h"
#include "graphics/rhi/shader_access.h"
#include "graphics/assets/shader.h"
#include "forward.h"

//todo establish naming convention for constants and enums

enum Cull : DrawCommandState {
	Cull_Offset = 0,
	Cull_Back = (0 << Cull_Offset) + 0,
	Cull_Front = (0 << Cull_Offset) + 1,
	Cull_None = (0 << Cull_Offset) + 2
};

enum DepthFunc : DrawCommandState {
	DepthFunc_Offset = 2,
	DepthFunc_Lequal = 0 << DepthFunc_Offset,
	DepthFunc_Less = 1 << DepthFunc_Offset,
	DepthFunc_None = 2 << DepthFunc_Offset,
	DepthFunc_Always = 3 << DepthFunc_Offset,
};

enum StencilFunc : DrawCommandState {
	StencilFunc_Offset = 4,
	StencilFunc_None = 0 << StencilFunc_Offset,
	StencilFunc_Equal = 1 << StencilFunc_Offset,
	StencilFunc_Nequal = 2 << StencilFunc_Offset,
	StencilFunc_Always = 3 << StencilFunc_Offset
};

enum ColorMask : DrawCommandState {
	ColorMask_Offset = 8,
	ColorMask_All = 0 << ColorMask_Offset,
	ColorMask_None = 1 << ColorMask_Offset,
};

enum DepthMask : DrawCommandState {
	DepthMask_Offset = 9,
	DepthMask_Write = 0 << DepthMask_Offset,
	DepthMask_None = 1 << DepthMask_Offset,
};

enum StencilOp : DrawCommandState {
	StencilOp_Offset = 10,
	StencilOp_Keep_Keep_Replace = 0 << StencilOp_Offset
};

enum PolyMode : DrawCommandState {
	PolyMode_Offset = 12,
	PolyMode_Fill = 0 << PolyMode_Offset,
	PolyMode_Wireframe = 1 << PolyMode_Offset
};

enum StencilMask : DrawCommandState {
	StencilMask_Offset = 14
};

enum WireframeLineWidth : DrawCommandState {
	WireframeLineWidth_Offset = 22
};

enum BlendMode : DrawCommandState {
	BlendMode_Offset = 30,
	BlendMode_None = 0 << BlendMode_Offset,
	BlendMode_One_Minus_Src_Alpha = 1 << BlendMode_Offset
};

enum DynamicState : DrawCommandState {
	DynamicState_Offset = 32,
	DynamicState_LineWidth = 1ull << (DynamicState_Offset + 0),
	DynamicState_Viewport  = 1ull << (DynamicState_Offset + 1),
	DynamicState_DepthBias = 1ull << (DynamicState_Offset + 2),
};

enum PrimitiveType : DrawCommandState {
	PrimitiveType_Offset        = 36,
	PrimitiveType_TriangleList  = 0ull << PrimitiveType_Offset,
	PrimitiveType_TriangleStrip = 1ull << PrimitiveType_Offset,
	PrimitiveType_TriangleFan   = 2ull << PrimitiveType_Offset,
	PrimitiveType_PointList     = 3ull << PrimitiveType_Offset,
	PrimitiveType_LineList      = 4ull << PrimitiveType_Offset,
	PrimitiveType_LineStrip     = 5ull << PrimitiveType_Offset
};

inline uint decode_DrawState(u64 offset, DrawCommandState state, uint bits) {
	return (state >> offset) & ((1 << bits) - 1);
}

const DrawCommandState default_draw_state = Cull_None | DepthFunc_Lequal;
const DrawCommandState draw_draw_over = Cull_None | DepthFunc_None;

enum PipelineStage {
	TOP_OF_PIPE_BIT = 1 << 0,
	DRAW_INDIRECT_BIT = 1 << 2,
	VERTEX_INPUT_BIT = 1 << 3,
	VERTEX_SHADER_BIT = 1 << 4,
	TESSELLATION_CONTROL_SHADER_BIT = 1 << 5,
	TESSELLATION_EVALUATION_SHADER_BIT = 1 << 6,
	GEOMETRY_SHADER_BIT = 1 << 7,
	FRAGMENT_SHADER_BIT = 1 << 8,
	EARLY_FRAGMENT_TESTS_BIT = 1 << 9,
	LATE_FRAGMENT_TESTS_BIT = 1 << 10,


	BOTTOM_OF_PIPE_BIT = 0 << 10,
};

inline PipelineStage operator|(PipelineStage a, PipelineStage b) {
	return (PipelineStage)((uint)a | (uint)b);
}

inline void operator|=(PipelineStage& a, PipelineStage b) {
	a = a | b;
}

struct PushConstantRange {
	union {
		struct {
			u8 offset;
			u8 size;
		};
		u16 packed = 0;
	};
};

//const u64 NATIVE_RENDER_PASS = 1ull << 63;

enum PushConstantStage {
	PushConstant_Vertex, 
	PushConstant_Fragment
};

struct PipelineDesc {
	shader_handle shader;
	Shader_Flags shader_flags = SHADER_INSTANCED;
	u64 render_pass;
	uint subpass = 0;
	VertexLayout vertex_layout = VERTEX_LAYOUT_DEFAULT;
	InstanceLayout instance_layout = INSTANCE_LAYOUT_MAT4X4;
	PushConstantRange range[2];

	inline bool operator==(const PipelineDesc& other) const {
		return shader.id == other.shader.id
			&& shader_flags == other.shader_flags
			&& render_pass == other.render_pass
			&& subpass == other.subpass
			&& vertex_layout == other.vertex_layout
			&& instance_layout == other.instance_layout
			&& range[0].packed == other.range[0].packed
			&& range[1].packed == other.range[1].packed;
	}

	inline bool operator!=(const PipelineDesc& other) const {
		return !(*this == other);
	}
};

struct GraphicsPipelineDesc : PipelineDesc {
	DrawCommandState state = 0;

	inline bool operator==(const GraphicsPipelineDesc& other) const {
		return *(PipelineDesc*)this == other && state == other.state;
	}

	inline bool operator!=(const GraphicsPipelineDesc& other) const {
		return !(*this == other);
	}
};

struct ComputePipelineDesc : PipelineDesc {};

ENGINE_API void reload_Pipeline(const GraphicsPipelineDesc&);
ENGINE_API pipeline_layout_handle query_Layout(slice<descriptor_set_handle> descriptors);
ENGINE_API pipeline_handle query_Pipeline(const GraphicsPipelineDesc&);
ENGINE_API pipeline_handle query_Pipeline(const ComputePipelineDesc&);
