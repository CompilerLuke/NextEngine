#include "stdafx.h"
#include "graphics/renderer/material_system.h"
#include "graphics/assets/asset_manager.h"
#include "core/io/logger.h"

REFLECT_UNION_BEGIN(Param)
REFLECT_UNION_FIELD(loc)
REFLECT_UNION_CASE_BEGIN()
REFLECT_UNION_CASE(vec3)
REFLECT_UNION_CASE(vec2)
REFLECT_UNION_CASE(matrix)
REFLECT_UNION_CASE(image)
REFLECT_UNION_CASE(cubemap)
REFLECT_UNION_CASE(integer)
REFLECT_UNION_CASE(real)
REFLECT_UNION_CASE(channel3)
REFLECT_UNION_CASE(channel2)
REFLECT_UNION_CASE(channel1)
REFLECT_UNION_CASE(time)
REFLECT_UNION_END()

REFLECT_STRUCT_BEGIN(Param::Channel3)
REFLECT_STRUCT_MEMBER(image)
REFLECT_STRUCT_MEMBER(color)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Param::Channel2)
REFLECT_STRUCT_MEMBER(image)
REFLECT_STRUCT_MEMBER(value)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Param::Channel1)
REFLECT_STRUCT_MEMBER(image)
REFLECT_STRUCT_MEMBER(value)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Material)
REFLECT_STRUCT_MEMBER(shader)
REFLECT_STRUCT_MEMBER(params)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Materials)
REFLECT_STRUCT_MEMBER(materials)
REFLECT_STRUCT_END()

Param::Param() {};

Param make_Param_Int(uniform_handle loc, int num) {
	Param param;
	param.loc = loc;
	param.type = Param_Int;
	param.integer = num;
	return param;
}

Param make_Param_Float(uniform_handle loc, float num) {
	Param param;
	param.loc = loc;
	param.type = Param_Float;
	param.real = num;
	return param;
}

Param make_Param_Vec3(uniform_handle loc, glm::vec3 vec) {
	Param param;
	param.loc = loc;
	param.type = Param_Vec3;
	param.vec3 = vec;
	return param;
}

Param make_Param_Vec2(uniform_handle loc, glm::vec2 vec) {
	Param param;
	param.loc = loc;
	param.type = Param_Vec2;
	param.vec2 = vec;
	return param;
}

Param make_Param_Cubemap(uniform_handle loc, cubemap_handle id) {
	Param param;
	param.loc = loc;
	param.type = Param_Cubemap;
	param.cubemap = id;
	return param;
}

Param make_Param_Image(uniform_handle loc, texture_handle id) {
	Param param;
	param.loc = loc;
	param.type = Param_Image;
	param.image = id;
	return param;
}

vector<Param> make_SubstanceMaterial(TextureManager& textures, ShaderManager& shaders, string_view folder, string_view name) {
	auto shad = shaders.load_get("shaders/pbr.vert", "shaders/pbr.frag");
	
	return {
		make_Param_Image(shad->location("material.diffuse"), textures.load(format(folder, "\\", name, "_basecolor.jpg"))),
		make_Param_Image(shad->location("material.metallic"), textures.load(format(folder, "\\", name, "_metallic.jpg"))),
		make_Param_Image(shad->location("material.roughness"), textures.load(format(folder, "\\", name, "_roughness.jpg"))),
		make_Param_Image(shad->location("material.normal"), textures.load(format(folder, "\\", name, "_normal.jpg"))),
		make_Param_Vec2(shad->location("transformUVs"), glm::vec2(100, 100))
	};
}

Material::Material() {}

Material::Material(shader_handle shader) : shader(shader) {}

void Material::set_int(ShaderManager& manager, string_view name, int value) {
	params.append(make_Param_Int(manager.location(shader, name), value));
}

void Material::set_float(ShaderManager& manager, string_view name, float value) {
	params.append(make_Param_Float(manager.location(shader, name), value));
}

void Material::set_vec2(ShaderManager& manager, string_view name, glm::vec2 value) {
	params.append(make_Param_Vec2(manager.location(shader, name), value));
}

void Material::set_vec3(ShaderManager& manager, string_view name, glm::vec3 value) {
	params.append(make_Param_Vec3(manager.location(shader, name), value));
}

void Material::set_image(ShaderManager& manager, string_view name, texture_handle value) {
	params.append(make_Param_Image(manager.location(shader, name), value));
}

void Material::set_cubemap(ShaderManager& manager, string_view name, cubemap_handle value) {
	params.append(make_Param_Cubemap(manager.location(shader, name), value));
}

void Material::retarget_from(AssetManager& manager, material_handle mat_handle) {
	Material* old_mat = manager.materials.get(mat_handle);
	
	Shader* old_shader = manager.shaders.get(old_mat->shader);

	for (Param p : old_mat->params) {
		p.loc = manager.shaders.location(shader, old_shader->uniforms[p.loc.id].name);
		params.append(p);
	}
}

void Material::set_flag(ShaderManager& manager, string_view flag, bool enabled) {
	this->shader_flags ^= (-enabled ^ this->shader_flags) & manager.get(shader)->get_flag(flag);
}

void Material::set_channel3(ShaderManager& shaders, string_view name, glm::vec3 value, texture_handle tex) {
	//if (tex.id == INVALID_HANDLE) {
	//	tex = texture::load("solid_white.png");
	//}
	
	Param param;
	param.loc = shaders.location(shader, name);
	param.type = Param_Channel3;
	param.channel3.color = value;
	param.channel3.image = tex;
	param.channel3.scalar_loc = shaders.location(shader, format(name, "_scalar"));

	params.append(param);
}