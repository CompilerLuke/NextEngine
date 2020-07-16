#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "graphics/rhi/forward.h"
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
};
 
REFL
struct MaterialDesc {	
	shader_handle shader = { INVALID_HANDLE };
	enum Mode { Static, Update } mode;
	DrawCommandState draw_state;
	array<10, ParamDesc> params;
	uint flags;
};

ENGINE_API void mat_flag(MaterialDesc&, string_view, bool);
ENGINE_API void mat_int(MaterialDesc&, string_view, int);
ENGINE_API void mat_float(MaterialDesc&, string_view, float);
ENGINE_API void mat_vec2(MaterialDesc&, string_view, glm::vec2);
ENGINE_API void mat_vec3(MaterialDesc&, string_view, glm::vec3);
ENGINE_API void mat_image(MaterialDesc&, string_view, texture_handle);
ENGINE_API void mat_cubemap(MaterialDesc&, string_view, cubemap_handle);
ENGINE_API void mat_channel3(MaterialDesc&, string_view, glm::vec3, texture_handle img = { INVALID_HANDLE });
ENGINE_API void mat_channel2(MaterialDesc&, string_view, glm::vec2, texture_handle img = { INVALID_HANDLE });
ENGINE_API void mat_channel1(MaterialDesc&, string_view, float, texture_handle img = { INVALID_HANDLE });

ENGINE_API material_handle make_Material(MaterialDesc&);
ENGINE_API MaterialDesc* material_desc(material_handle);
ENGINE_API void replace_Material(material_handle, MaterialDesc&);

COMP
struct Materials {
	vector<material_handle> materials;
};

material_handle make_SubstanceMaterial(string_view folder, string_view);

namespace refl { struct Union; }

//todo make this inlineable, as it will improve serialization performance significantly
void write_ParamDesc_to_buffer(struct SerializerBuffer& buffer, ParamDesc& param);
void read_ParamDesc_from_buffer(struct DeserializerBuffer& buffer, ParamDesc* param);
refl::Union* get_ParamDesc_type();