#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "graphics/rhi/forward.h"
#include "core/container/array.h"
#include "core/container/vector.h"
#include "core/container/sstring.h"
#include "core/container/string_view.h"
#include "engine/handle.h"
#include "graphics/pass/pass.h"

#define REQUIRES_TIME (1 << 0)
#define DEPENDENCY_PARAMETER (1 << 1)

struct ShaderManager;
struct Assets;
struct GraphicsPipelineDesc;

REFL
enum Param_Type {
	Param_Vec3,
	Param_Vec2,
	Param_Mat4x4,
	Param_Image,
	Param_Cubemap,
	Param_Int,
	Param_Float,
	Param_Channel3, Param_Channel2, Param_Channel1, Param_Channel4,
};

REFL
struct ParamDesc {
	Param_Type     type;
	sstring        name;
	uint           image;
	union {
		int        integer;
		float      real;
		glm::vec2  vec2;
		glm::vec3  vec3;
		glm::vec4  vec4;
	};

	ParamDesc() {}
	ParamDesc(const ParamDesc& other) { memcpy(this, &other, sizeof(ParamDesc)); }
};
 
REFL
struct MaterialDesc {	
	shader_handle shader = { INVALID_HANDLE };
	enum Mode { Static, Update } mode;
	DrawCommandState draw_state;
	array<10, ParamDesc> params;
	uint flags;


    ENGINE_API void mat_flag(string_view, bool);

    ENGINE_API void mat_int(string_view, int);

    ENGINE_API void mat_float(string_view, float);

    ENGINE_API void mat_vec2(string_view, glm::vec2);

    ENGINE_API void mat_vec3(string_view, glm::vec3);

    ENGINE_API void mat_image(string_view, texture_handle);

    ENGINE_API void mat_cubemap(string_view, cubemap_handle);

    ENGINE_API void mat_channel4(string_view, glm::vec4, texture_handle img = {INVALID_HANDLE});

    ENGINE_API void mat_channel3(string_view, glm::vec3, texture_handle img = {INVALID_HANDLE});

    ENGINE_API void mat_channel2(string_view, glm::vec2, texture_handle img = {INVALID_HANDLE});

    ENGINE_API void mat_channel1(string_view, float, texture_handle img = {INVALID_HANDLE});
};

struct MaterialPipelineInfo {
    shader_handle shader;
    DrawCommandState state;
};
ENGINE_API material_handle make_Material(MaterialDesc&, bool serialized = false);
ENGINE_API void make_Material(material_handle, MaterialDesc&);
ENGINE_API MaterialPipelineInfo material_pipeline_info(material_handle);
ENGINE_API void mat_pipeline_desc(GraphicsPipelineDesc&, material_handle, RenderPass::ID, uint subpass);
ENGINE_API pipeline_handle query_pipeline(material_handle, RenderPass::ID, uint subpass);
ENGINE_API void update_Material(material_handle, MaterialDesc& from, MaterialDesc& to);

COMP
struct Materials {
	vector<material_handle> materials;
};

ENGINE_API material_handle mat_by_index(Materials&, uint material_id);
material_handle make_SubstanceMaterial(string_view folder, string_view);

