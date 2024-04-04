#include "graphics/assets/material.h"
#include "graphics/assets/assets.h"
#include "core/io/logger.h"

material_handle make_SubstanceMaterial(Assets& assets, string_view folder, string_view name) {
	auto shad = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");
	
	MaterialDesc mat{ shad };
	mat.mat_image("material.diffuse", load_Texture(tformat(folder, "\\", name, "_basecolor.jpg")));
	mat.mat_image("material.metallic", load_Texture(tformat(folder, "\\", name, "_metallic.jpg")));
	mat.mat_image("material.roughness", load_Texture(tformat(folder, "\\", name, "_roughness.jpg")));
	mat.mat_image("material.normal", load_Texture(tformat(folder, "\\", name, "_normal.jpg")));
	mat.mat_vec2("transformUVs", glm::vec2(1, 1));

	return make_Material(mat);
}

void MaterialDesc::mat_int(string_view name, int value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Int;
	param.integer = value;

	params.append(param);
}

void MaterialDesc::mat_float(string_view name, float value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Float;
	param.real = value;

	params.append(param);
}

void MaterialDesc::mat_vec2(string_view name, glm::vec2 value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Vec2;
	param.vec2 = value;
	
	params.append(param);
}

void MaterialDesc::mat_vec3(string_view name, glm::vec3 value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Vec3;
	param.vec3 = value;
	
	params.append(param);
}

void MaterialDesc::mat_image(string_view name, texture_handle value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Image;
	param.image = value.id;

	params.append(param);
}

void MaterialDesc::mat_cubemap(string_view name, cubemap_handle value) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Cubemap;
	param.image = value.id;
	param.vec3 = glm::vec3(1);
	
	params.append(param);
}

void MaterialDesc::mat_channel1(string_view name, float value, texture_handle tex) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Channel1;
	param.real = value;
	param.image = tex.id;

	params.append(param);
}


void MaterialDesc::mat_channel2(string_view name, glm::vec2 value, texture_handle tex) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Channel2;
	param.vec2 = value;
	param.image = tex.id;

	params.append(param);
}

// static_assert(std::is_same_v<glm::mat4x4, glm::mat<4, 4, float, (glm::qualifier)0>>);

void MaterialDesc::mat_channel3(string_view name, glm::vec3 value, texture_handle tex) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Channel3;
	param.vec3 = value;
	param.image = tex.id;

	params.append(param);
}

void MaterialDesc::mat_channel4(string_view name, glm::vec4 value, texture_handle tex) {
	ParamDesc param;
	param.name = name;
	param.type = Param_Channel4;
	param.vec4 = value;
	param.image = tex.id;

	params.append(param);
}

material_handle mat_by_index(Materials& materials, uint material_id) {
	if (materials.materials.length <= material_id) { //todo add this as a preprocess pass
		materials.materials.resize(material_id + 1);
	}
	material_handle mat_handle = materials.materials[material_id];
	if (mat_handle.id == INVALID_HANDLE) mat_handle = default_materials.missing;
	return mat_handle;
}