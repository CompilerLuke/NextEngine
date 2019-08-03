#include "stdafx.h"
#include "graphics/materialSystem.h"
#include "graphics/texture.h"
#include "graphics/rhi.h"
#include "logger/logger.h"

REFLECT_GENERIC_STRUCT_BEGIN(Handle<Material>)
REFLECT_STRUCT_MEMBER(id)
REFLECT_STRUCT_END()

REFLECT_UNION_BEGIN(Param)
REFLECT_UNION_FIELD(loc)
REFLECT_UNION_CASE(vec3)
REFLECT_UNION_CASE(vec2)
REFLECT_UNION_CASE(matrix)
REFLECT_UNION_CASE(image)
REFLECT_UNION_CASE(cubemap)
REFLECT_UNION_CASE(integer)
REFLECT_UNION_CASE(real)
REFLECT_UNION_END()

REFLECT_STRUCT_BEGIN(Material)
REFLECT_STRUCT_MEMBER(shader)
REFLECT_STRUCT_MEMBER(params)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Materials)
REFLECT_STRUCT_MEMBER(materials)
REFLECT_STRUCT_END()

Param::Param() {};

Param make_Param_Int(Handle<Uniform> loc, int num) {
	Param param;
	param.loc = loc;
	param.type = Param_Int;
	param.integer = num;
	return param;
}

Param make_Param_Float(Handle<Uniform> loc, float num) {
	Param param;
	param.loc = loc;
	param.type = Param_Float;
	param.real = num;
	return param;
}

Param make_Param_Vec3(Handle<Uniform> loc, glm::vec3 vec) {
	Param param;
	param.loc = loc;
	param.type = Param_Vec3;
	param.vec3 = vec;
	return param;
}

Param make_Param_Vec2(Handle<Uniform> loc, glm::vec2 vec) {
	Param param;
	param.loc = loc;
	param.type = Param_Vec2;
	param.vec2 = vec;
	return param;
}

Param make_Param_Cubemap(Handle<Uniform> loc, Handle<Cubemap> id) {
	Param param;
	param.loc = loc;
	param.type = Param_Cubemap;
	param.cubemap = id;
	return param;
}

Param make_Param_Image(Handle<Uniform> loc, Handle<Texture> id) {
	Param param;
	param.loc = loc;
	param.type = Param_Image;
	param.image = id;
	return param;
}

vector<Param> make_SubstanceMaterial(StringView folder, StringView name) {
	auto shad = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");
	
	return {
		make_Param_Image(location(shad, "material.diffuse"), load_Texture(format(folder, "\\", name, "_basecolor.jpg"))),
		make_Param_Image(location(shad, "material.metallic"), load_Texture(format(folder, "\\", name, "_metallic.jpg"))),
		make_Param_Image(location(shad, "material.roughness"),load_Texture(format(folder, "\\", name, "_roughness.jpg"))),
		make_Param_Image(location(shad, "material.normal"), load_Texture(format(folder, "\\", name, "_normal.jpg"))),
		make_Param_Vec2(location(shad, "transformUVs"), glm::vec2(100, 100))
	};
}


