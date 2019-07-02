#include "stdafx.h"
#include "graphics/materialSystem.h"
#include "graphics/texture.h"

REFLECT_UNION_BEGIN(Param)
REFLECT_UNION_FIELD(loc)
REFLECT_UNION_CASE(vec3)
REFLECT_UNION_CASE(vec2)
REFLECT_UNION_CASE(matrix)
REFLECT_UNION_CASE(image)
REFLECT_UNION_CASE(cubemap)
REFLECT_UNION_CASE(integer)
REFLECT_UNION_END()

REFLECT_STRUCT_BEGIN(Material)
REFLECT_STRUCT_MEMBER(name)
REFLECT_STRUCT_MEMBER_TAG(shader, reflect::ShaderIDTag)
REFLECT_STRUCT_MEMBER(params)
REFLECT_STRUCT_MEMBER(state)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Materials)
REFLECT_STRUCT_MEMBER(materials)
REFLECT_STRUCT_END()

Material* material_by_name(vector<Material>& materials, const std::string& name) {
	for (int i = 0; i < materials.length; i++) {
		if (materials[i].name == name) return &materials[i];
	}
	return NULL;
}

Param::Param() {};

Param make_Param_Int(Handle<Uniform> loc, int num) {
	Param param;
	param.loc = loc;
	param.type = Param_Int;
	param.integer = num;
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

vector<Param> make_SubstanceMaterial(World& world, const std::string& folder, const std::string& name) {
	auto shad = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");
	
	return {
		make_Param_Image(location(shad, "material.diffuse"), load_Texture(folder + "\\" + name + "_basecolor.jpg")),
		make_Param_Image(location(shad, "material.metallic"), load_Texture(folder + "\\" + name + "_metallic.jpg")),
		make_Param_Image(location(shad, "material.roughness"),load_Texture(folder + "\\" + name + "_roughness.jpg")),
		make_Param_Image(location(shad, "material.normal"), load_Texture(folder + "\\" + name + "_normal.jpg")),
		make_Param_Vec2(location(shad, "transformUVs"), glm::vec2(100, 100))
	};
}


