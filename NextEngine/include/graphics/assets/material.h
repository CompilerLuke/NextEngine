#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "core/reflection.h"
#include "graphics/rhi/draw.h"
#include "core/container/array.h"
#include "core/container/vector.h"
#include "core/container/string_buffer.h"
#include "core/container/string_view.h"
#include "core/handle.h"

#define REQUIRES_TIME (1 << 0)

struct ShaderManager;
struct Assets;

enum Param_Type {
	Param_Vec3,
	Param_Vec2,
	Param_Mat4x4,
	Param_Image,
	Param_Cubemap,
	Param_Int,
	Param_Float,
	Param_Channel3, Param_Channel2, Param_Channel1, 
	Param_Time
};

struct ParamDesc {
	Param_Type     type;
	sstring        name;
	uint           image;
	union {
		int        integer;
		float      real;
		glm::vec2  vec2;
		glm::vec3  vec3;
	};

	ParamDesc() {};
	ParamDesc(const ParamDesc& other) { memcpy(this, &other, sizeof(ParamDesc)); }

	REFLECT_UNION(ENGINE_API)
};

struct MaterialDesc {	
	shader_handle shader = { INVALID_HANDLE };
	array<10, ParamDesc> params;
	uint flags;
};

void ENGINE_API mat_flag(MaterialDesc& desc, string_view, bool);
void ENGINE_API mat_int(MaterialDesc& desc, string_view, int);
void ENGINE_API mat_float(MaterialDesc& desc, string_view, float);
void ENGINE_API mat_vec2(MaterialDesc& desc, string_view, glm::vec2);
void ENGINE_API mat_vec3(MaterialDesc& desc, string_view, glm::vec3);
void ENGINE_API mat_image(MaterialDesc& desc, string_view, texture_handle);
void ENGINE_API mat_channel3(MaterialDesc& desc, string_view, glm::vec3, texture_handle img = { INVALID_HANDLE });
void ENGINE_API mat_channel2(MaterialDesc& desc, string_view, glm::vec2, texture_handle img = { INVALID_HANDLE });
void ENGINE_API mat_channel1(MaterialDesc& desc, string_view, float, texture_handle img = { INVALID_HANDLE });

ENGINE_API material_handle make_Material(Assets&, MaterialDesc&);
ENGINE_API MaterialDesc* material_desc(Assets&, material_handle);
ENGINE_API void replace_Material(Assets&, material_handle, MaterialDesc&);

struct Materials {
	vector<material_handle> materials;

	REFLECT(ENGINE_API)
};

material_handle make_SubstanceMaterial(string_view folder, string_view);