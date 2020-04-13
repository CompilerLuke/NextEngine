#include "stdafx.h"
#include "graphics/assets/material.h"
#include "graphics/assets/assets.h"
#include "core/io/logger.h"

REFLECT_UNION_BEGIN(ParamDesc)
REFLECT_UNION_FIELD(name)
REFLECT_UNION_FIELD(name)
REFLECT_UNION_FIELD(image)
REFLECT_UNION_CASE_BEGIN()
REFLECT_UNION_CASE(integer)
REFLECT_UNION_CASE(real)
REFLECT_UNION_CASE(vec2)
REFLECT_UNION_CASE(vec3)
REFLECT_UNION_END()

REFLECT_STRUCT_BEGIN(Materials)
REFLECT_STRUCT_MEMBER(materials)
REFLECT_STRUCT_END()

material_handle make_SubstanceMaterial(Assets& assets, string_view folder, string_view name) {
	auto shad = load_Shader(assets, "shaders/pbr.vert", "shaders/pbr.frag");
	
	MaterialDesc mat{ shad };
	mat_image(mat, "material.diffuse", load_Texture(assets, format(folder, "\\", name, "_basecolor.jpg")));
	mat_image(mat, "material.metallic", load_Texture(assets, format(folder, "\\", name, "_metallic.jpg")));
	mat_image(mat, "material.roughness", load_Texture(assets, format(folder, "\\", name, "_roughness.jpg")));
	mat_image(mat, "material.normal", load_Texture(assets, format(folder, "\\", name, "_normal.jpg")));
	mat_vec2(mat, "transformUVs", glm::vec2(1, 1));

	return make_Material(assets, mat);
}

void mat_int(MaterialDesc& desc, string_view name, int value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Int;
	param.integer = value;

	desc.params.append(param);
}

void mat_float(MaterialDesc& desc, string_view name, float value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Float;
	param.real = value;

	desc.params.append(param);
}

void mat_vec2(MaterialDesc& desc, string_view name, glm::vec2 value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Vec2;
	param.vec2 = value;
	
	desc.params.append(param);
}

void mat_vec3(MaterialDesc& desc, string_view name, glm::vec3 value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Vec3;
	param.vec3 = value;
	
	desc.params.append(param);
}

void mat_image(MaterialDesc& desc, string_view name, texture_handle value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Image;
	param.image = value.id;

	desc.params.append(param);
}

void mat_cubemap(MaterialDesc& desc, string_view name, cubemap_handle value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Cubemap;
	param.image = value.id;
	
	desc.params.append(param);
}

void mat_channel3(MaterialDesc& desc, string_view name, glm::vec3 value, texture_handle tex) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Channel3;
	param.vec3 = value;
	param.image = tex.id;

	desc.params.append(param);
}