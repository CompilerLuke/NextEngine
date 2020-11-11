#include "engine/types.h"
#include "components/transform.h"
#include "components/camera.h"
#include "components/flyover.h"
#include "components/skybox.h"
#include "components/terrain.h"
#include "components/lights.h"
#include "components/grass.h"
#include "graphics/rhi/forward.h"
#include "engine/handle.h"
#include "physics/physics.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/assets_store.h"
#include "graphics/assets/material.h"
#include "graphics/assets/model.h"
#include "graphics/assets/shader.h"
#include "graphics/assets/texture.h"
#include "ecs/component_ids.h"
#include "ecs/ecs.h"
#include "ecs/flags.h"
#include "ecs/id.h"
#include "ecs/system.h"
#include <glm/vec2.hpp>

refl::Enum init_MaterialDesc_Mode() {
	refl::Enum type("Mode", sizeof(MaterialDesc::Mode));
	type.values.append({ "Static", MaterialDesc::Mode::Static });
	return type;
}

refl::Enum* get_MaterialDesc_Mode_type() {
	static refl::Enum type = init_MaterialDesc_Mode();
	return &type;
}
refl::Alias* get_DrawCommandState_type() {
    static refl::Alias type("DrawCommandState", get_u64_type());
    return &type;
}

void write_DrawCommandState_to_buffer(SerializerBuffer& buffer, DrawCommandState& data) {
    write_n_to_buffer(buffer, &data, sizeof(u64));
}
void read_DrawCommandState_from_buffer(DeserializerBuffer& buffer, DrawCommandState& data) {
    read_n_from_buffer(buffer, &data, sizeof(u64));
}
refl::Alias* get_shader_flags_type() {
    static refl::Alias type("shader_flags", get_u64_type());
    return &type;
}

void write_shader_flags_to_buffer(SerializerBuffer& buffer, shader_flags& data) {
    write_n_to_buffer(buffer, &data, sizeof(u64));
}
void read_shader_flags_from_buffer(DeserializerBuffer& buffer, shader_flags& data) {
    read_n_from_buffer(buffer, &data, sizeof(u64));
}
refl::Alias* get_ID_type() {
    static refl::Alias type("ID", get_uint_type());
    return &type;
}

void write_ID_to_buffer(SerializerBuffer& buffer, ID& data) {
    write_n_to_buffer(buffer, &data, sizeof(uint));
}
void read_ID_from_buffer(DeserializerBuffer& buffer, ID& data) {
    read_n_from_buffer(buffer, &data, sizeof(uint));
}
refl::Alias* get_Layermask_type() {
    static refl::Alias type("Layermask", get_u64_type());
    return &type;
}

void write_Layermask_to_buffer(SerializerBuffer& buffer, Layermask& data) {
    write_n_to_buffer(buffer, &data, sizeof(u64));
}
void read_Layermask_from_buffer(DeserializerBuffer& buffer, Layermask& data) {
    read_n_from_buffer(buffer, &data, sizeof(u64));
}
refl::Alias* get_Archetype_type() {
    static refl::Alias type("Archetype", get_u64_type());
    return &type;
}

void write_Archetype_to_buffer(SerializerBuffer& buffer, Archetype& data) {
    write_n_to_buffer(buffer, &data, sizeof(u64));
}
void read_Archetype_from_buffer(DeserializerBuffer& buffer, Archetype& data) {
    read_n_from_buffer(buffer, &data, sizeof(u64));
}
refl::Alias* get_EntityFlags_type() {
    static refl::Alias type("EntityFlags", get_u64_type());
    return &type;
}

void write_EntityFlags_to_buffer(SerializerBuffer& buffer, EntityFlags& data) {
    write_n_to_buffer(buffer, &data, sizeof(u64));
}
void read_EntityFlags_from_buffer(DeserializerBuffer& buffer, EntityFlags& data) {
    read_n_from_buffer(buffer, &data, sizeof(u64));
}
refl::Enum init_Param_Type() {
	refl::Enum type("Param_Type", sizeof(Param_Type));
	type.values.append({ "Param_Vec3", Param_Type::Param_Vec3 });
	type.values.append({ "Param_Vec2", Param_Type::Param_Vec2 });
	type.values.append({ "Param_Mat4x4", Param_Type::Param_Mat4x4 });
	type.values.append({ "Param_Image", Param_Type::Param_Image });
	type.values.append({ "Param_Cubemap", Param_Type::Param_Cubemap });
	type.values.append({ "Param_Int", Param_Type::Param_Int });
	type.values.append({ "Param_Float", Param_Type::Param_Float });
	type.values.append({ "Param_Channel3", Param_Type::Param_Channel3 });
	type.values.append({ "Param_Channel2", Param_Type::Param_Channel2 });
	type.values.append({ "Param_Channel1", Param_Type::Param_Channel1 });
	type.values.append({ "Param_Channel4", Param_Type::Param_Channel4 });
	return type;
}

refl::Enum* get_Param_Type_type() {
	static refl::Enum type = init_Param_Type();
	return &type;
}
refl::Struct init_Transform() {
	refl::Struct type("Transform", sizeof(Transform));
	type.fields.append({"position", offsetof(Transform, position), get_vec3_type()});
	type.fields.append({"rotation", offsetof(Transform, rotation), get_quat_type()});
	type.fields.append({"scale", offsetof(Transform, scale), get_vec3_type()});
	return type;
}

void write_Transform_to_buffer(SerializerBuffer& buffer, Transform& data) {
    write_n_to_buffer(buffer, &data, sizeof(Transform));
}

void read_Transform_from_buffer(DeserializerBuffer& buffer, Transform& data) {
    read_n_from_buffer(buffer, &data, sizeof(Transform));
}

refl::Struct* get_Transform_type() {
	static refl::Struct type = init_Transform();
	return &type;
}

refl::Struct init_StaticTransform() {
	refl::Struct type("StaticTransform", sizeof(StaticTransform));
	type.fields.append({"model_matrix", offsetof(StaticTransform, model_matrix), get_mat4_type()});
	return type;
}

void write_StaticTransform_to_buffer(SerializerBuffer& buffer, StaticTransform& data) {
    write_n_to_buffer(buffer, &data, sizeof(StaticTransform));
}

void read_StaticTransform_from_buffer(DeserializerBuffer& buffer, StaticTransform& data) {
    read_n_from_buffer(buffer, &data, sizeof(StaticTransform));
}

refl::Struct* get_StaticTransform_type() {
	static refl::Struct type = init_StaticTransform();
	return &type;
}

refl::Struct init_LocalTransform() {
	refl::Struct type("LocalTransform", sizeof(LocalTransform));
	type.fields.append({"position", offsetof(LocalTransform, position), get_vec3_type()});
	type.fields.append({"rotation", offsetof(LocalTransform, rotation), get_quat_type()});
	type.fields.append({"scale", offsetof(LocalTransform, scale), get_vec3_type()});
	type.fields.append({"owner", offsetof(LocalTransform, owner), get_ID_type()});
	return type;
}

void write_LocalTransform_to_buffer(SerializerBuffer& buffer, LocalTransform& data) {
    write_n_to_buffer(buffer, &data, sizeof(LocalTransform));
}

void read_LocalTransform_from_buffer(DeserializerBuffer& buffer, LocalTransform& data) {
    read_n_from_buffer(buffer, &data, sizeof(LocalTransform));
}

refl::Struct* get_LocalTransform_type() {
	static refl::Struct type = init_LocalTransform();
	return &type;
}

refl::Struct init_Camera() {
	refl::Struct type("Camera", sizeof(Camera));
	type.fields.append({"near_plane", offsetof(Camera, near_plane), get_float_type()});
	type.fields.append({"far_plane", offsetof(Camera, far_plane), get_float_type()});
	type.fields.append({"fov", offsetof(Camera, fov), get_float_type()});
	return type;
}

void write_Camera_to_buffer(SerializerBuffer& buffer, Camera& data) {
    write_n_to_buffer(buffer, &data, sizeof(Camera));
}

void read_Camera_from_buffer(DeserializerBuffer& buffer, Camera& data) {
    read_n_from_buffer(buffer, &data, sizeof(Camera));
}

refl::Struct* get_Camera_type() {
	static refl::Struct type = init_Camera();
	return &type;
}

refl::Struct init_Flyover() {
	refl::Struct type("Flyover", sizeof(Flyover));
	type.fields.append({"mouse_sensitivity", offsetof(Flyover, mouse_sensitivity), get_float_type()});
	type.fields.append({"movement_speed", offsetof(Flyover, movement_speed), get_float_type()});
	type.fields.append({"yaw", offsetof(Flyover, yaw), get_float_type()});
	type.fields.append({"pitch", offsetof(Flyover, pitch), get_float_type()});
	type.fields.append({"past_movement_speed", offsetof(Flyover, past_movement_speed), make_array_type(3, sizeof(array<3, glm::vec2>), get_vec2_type())});
	return type;
}

void write_Flyover_to_buffer(SerializerBuffer& buffer, Flyover& data) {
    write_n_to_buffer(buffer, &data, sizeof(Flyover));
}

void read_Flyover_from_buffer(DeserializerBuffer& buffer, Flyover& data) {
    read_n_from_buffer(buffer, &data, sizeof(Flyover));
}

refl::Struct* get_Flyover_type() {
	static refl::Struct type = init_Flyover();
	return &type;
}

refl::Struct init_Skybox() {
	refl::Struct type("Skybox", sizeof(Skybox));
	type.fields.append({"cubemap", offsetof(Skybox, cubemap), get_cubemap_handle_type()});
	return type;
}

void write_Skybox_to_buffer(SerializerBuffer& buffer, Skybox& data) {
    write_n_to_buffer(buffer, &data, sizeof(Skybox));
}

void read_Skybox_from_buffer(DeserializerBuffer& buffer, Skybox& data) {
    read_n_from_buffer(buffer, &data, sizeof(Skybox));
}

refl::Struct* get_Skybox_type() {
	static refl::Struct type = init_Skybox();
	return &type;
}

refl::Struct init_TerrainControlPoint() {
	refl::Struct type("TerrainControlPoint", sizeof(TerrainControlPoint));
	return type;
}

void write_TerrainControlPoint_to_buffer(SerializerBuffer& buffer, TerrainControlPoint& data) {
    write_n_to_buffer(buffer, &data, sizeof(TerrainControlPoint));
}

void read_TerrainControlPoint_from_buffer(DeserializerBuffer& buffer, TerrainControlPoint& data) {
    read_n_from_buffer(buffer, &data, sizeof(TerrainControlPoint));
}

refl::Struct* get_TerrainControlPoint_type() {
	static refl::Struct type = init_TerrainControlPoint();
	return &type;
}

refl::Struct init_TerrainSplat() {
	refl::Struct type("TerrainSplat", sizeof(TerrainSplat));
	type.fields.append({"hardness", offsetof(TerrainSplat, hardness), get_float_type()});
	type.fields.append({"min_height", offsetof(TerrainSplat, min_height), get_float_type()});
	type.fields.append({"max_height", offsetof(TerrainSplat, max_height), get_float_type()});
	type.fields.append({"material", offsetof(TerrainSplat, material), get_uint_type()});
	return type;
}

void write_TerrainSplat_to_buffer(SerializerBuffer& buffer, TerrainSplat& data) {
    write_n_to_buffer(buffer, &data, sizeof(TerrainSplat));
}

void read_TerrainSplat_from_buffer(DeserializerBuffer& buffer, TerrainSplat& data) {
    read_n_from_buffer(buffer, &data, sizeof(TerrainSplat));
}

refl::Struct* get_TerrainSplat_type() {
	static refl::Struct type = init_TerrainSplat();
	return &type;
}

refl::Struct init_TerrainMaterial() {
	refl::Struct type("TerrainMaterial", sizeof(TerrainMaterial));
	type.fields.append({"name", offsetof(TerrainMaterial, name), get_sstring_type()});
	type.fields.append({"diffuse", offsetof(TerrainMaterial, diffuse), get_texture_handle_type()});
	type.fields.append({"metallic", offsetof(TerrainMaterial, metallic), get_texture_handle_type()});
	type.fields.append({"roughness", offsetof(TerrainMaterial, roughness), get_texture_handle_type()});
	type.fields.append({"normal", offsetof(TerrainMaterial, normal), get_texture_handle_type()});
	type.fields.append({"height", offsetof(TerrainMaterial, height), get_texture_handle_type()});
	type.fields.append({"ao", offsetof(TerrainMaterial, ao), get_texture_handle_type()});
	return type;
}

void write_TerrainMaterial_to_buffer(SerializerBuffer& buffer, TerrainMaterial& data) {
    write_n_to_buffer(buffer, &data, sizeof(TerrainMaterial));
}

void read_TerrainMaterial_from_buffer(DeserializerBuffer& buffer, TerrainMaterial& data) {
    read_n_from_buffer(buffer, &data, sizeof(TerrainMaterial));
}

refl::Struct* get_TerrainMaterial_type() {
	static refl::Struct type = init_TerrainMaterial();
	return &type;
}

refl::Struct init_Terrain() {
	refl::Struct type("Terrain", sizeof(Terrain));
	type.fields.append({"width", offsetof(Terrain, width), get_uint_type()});
	type.fields.append({"height", offsetof(Terrain, height), get_uint_type()});
	type.fields.append({"size_of_block", offsetof(Terrain, size_of_block), get_uint_type()});
	type.fields.append({"show_control_points", offsetof(Terrain, show_control_points), get_bool_type()});
	type.fields.append({"heightmap", offsetof(Terrain, heightmap), get_texture_handle_type()});
	type.fields.append({"max_height", offsetof(Terrain, max_height), get_float_type()});
	type.fields.append({"materials", offsetof(Terrain, materials), make_vector_type(get_TerrainMaterial_type())});
	return type;
}

void write_Terrain_to_buffer(SerializerBuffer& buffer, Terrain& data) {
    write_n_to_buffer(buffer, &data.width, sizeof(uint));
    write_n_to_buffer(buffer, &data.height, sizeof(uint));
    write_n_to_buffer(buffer, &data.size_of_block, sizeof(uint));
    write_n_to_buffer(buffer, &data.show_control_points, sizeof(bool));
    write_texture_handle_to_buffer(buffer, data.heightmap);
    write_n_to_buffer(buffer, &data.max_height, sizeof(float));
    write_uint_to_buffer(buffer, data.materials.length);
	for (uint i = 0; i < data.materials.length; i++) {
         write_TerrainMaterial_to_buffer(buffer, data.materials[i]);
    }
}

void read_Terrain_from_buffer(DeserializerBuffer& buffer, Terrain& data) {
    read_n_from_buffer(buffer, &data.width, sizeof(uint));
    read_n_from_buffer(buffer, &data.height, sizeof(uint));
    read_n_from_buffer(buffer, &data.size_of_block, sizeof(uint));
    read_n_from_buffer(buffer, &data.show_control_points, sizeof(bool));
    read_texture_handle_from_buffer(buffer, data.heightmap);
    read_n_from_buffer(buffer, &data.max_height, sizeof(float));
    data.materials.resize(read_uint_from_buffer(buffer));
	for (uint i = 0; i < data.materials.length; i++) {
         read_TerrainMaterial_from_buffer(buffer, data.materials[i]);
    }
}

refl::Struct* get_Terrain_type() {
	static refl::Struct type = init_Terrain();
	return &type;
}

refl::Struct init_DirLight() {
	refl::Struct type("DirLight", sizeof(DirLight));
	type.fields.append({"direction", offsetof(DirLight, direction), get_vec3_type()});
	type.fields.append({"color", offsetof(DirLight, color), get_vec3_type()});
	return type;
}

void write_DirLight_to_buffer(SerializerBuffer& buffer, DirLight& data) {
    write_n_to_buffer(buffer, &data, sizeof(DirLight));
}

void read_DirLight_from_buffer(DeserializerBuffer& buffer, DirLight& data) {
    read_n_from_buffer(buffer, &data, sizeof(DirLight));
}

refl::Struct* get_DirLight_type() {
	static refl::Struct type = init_DirLight();
	return &type;
}

refl::Struct init_PointLight() {
	refl::Struct type("PointLight", sizeof(PointLight));
	type.fields.append({"color", offsetof(PointLight, color), get_vec3_type()});
	type.fields.append({"radius", offsetof(PointLight, radius), get_float_type()});
	return type;
}

void write_PointLight_to_buffer(SerializerBuffer& buffer, PointLight& data) {
    write_n_to_buffer(buffer, &data, sizeof(PointLight));
}

void read_PointLight_from_buffer(DeserializerBuffer& buffer, PointLight& data) {
    read_n_from_buffer(buffer, &data, sizeof(PointLight));
}

refl::Struct* get_PointLight_type() {
	static refl::Struct type = init_PointLight();
	return &type;
}

refl::Struct init_SkyLight() {
	refl::Struct type("SkyLight", sizeof(SkyLight));
	type.fields.append({"capture_scene", offsetof(SkyLight, capture_scene), get_bool_type()});
	type.fields.append({"cubemap", offsetof(SkyLight, cubemap), get_cubemap_handle_type()});
	type.fields.append({"irradiance", offsetof(SkyLight, irradiance), get_cubemap_handle_type()});
	type.fields.append({"prefilter", offsetof(SkyLight, prefilter), get_cubemap_handle_type()});
	return type;
}

void write_SkyLight_to_buffer(SerializerBuffer& buffer, SkyLight& data) {
    write_n_to_buffer(buffer, &data, sizeof(SkyLight));
}

void read_SkyLight_from_buffer(DeserializerBuffer& buffer, SkyLight& data) {
    read_n_from_buffer(buffer, &data, sizeof(SkyLight));
}

refl::Struct* get_SkyLight_type() {
	static refl::Struct type = init_SkyLight();
	return &type;
}

refl::Struct init_Grass() {
	refl::Struct type("Grass", sizeof(Grass));
	type.fields.append({"placement_model", offsetof(Grass, placement_model), get_model_handle_type()});
	type.fields.append({"cast_shadows", offsetof(Grass, cast_shadows), get_bool_type()});
	type.fields.append({"width", offsetof(Grass, width), get_float_type()});
	type.fields.append({"height", offsetof(Grass, height), get_float_type()});
	type.fields.append({"max_height", offsetof(Grass, max_height), get_float_type()});
	type.fields.append({"density", offsetof(Grass, density), get_float_type()});
	type.fields.append({"random_rotation", offsetof(Grass, random_rotation), get_float_type()});
	type.fields.append({"random_scale", offsetof(Grass, random_scale), get_float_type()});
	type.fields.append({"align_to_terrain_normal", offsetof(Grass, align_to_terrain_normal), get_bool_type()});
	type.fields.append({"positions", offsetof(Grass, positions), make_vector_type(get_vec3_type())});
	type.fields.append({"model_m", offsetof(Grass, model_m), make_vector_type(get_mat4_type())});
	return type;
}

void write_Grass_to_buffer(SerializerBuffer& buffer, Grass& data) {
    write_model_handle_to_buffer(buffer, data.placement_model);
    write_n_to_buffer(buffer, &data.cast_shadows, sizeof(bool));
    write_n_to_buffer(buffer, &data.width, sizeof(float));
    write_n_to_buffer(buffer, &data.height, sizeof(float));
    write_n_to_buffer(buffer, &data.max_height, sizeof(float));
    write_n_to_buffer(buffer, &data.density, sizeof(float));
    write_n_to_buffer(buffer, &data.random_rotation, sizeof(float));
    write_n_to_buffer(buffer, &data.random_scale, sizeof(float));
    write_n_to_buffer(buffer, &data.align_to_terrain_normal, sizeof(bool));
    write_uint_to_buffer(buffer, data.positions.length);
	for (uint i = 0; i < data.positions.length; i++) {
         write_vec3_to_buffer(buffer, data.positions[i]);
    }
    write_uint_to_buffer(buffer, data.model_m.length);
	for (uint i = 0; i < data.model_m.length; i++) {
         write_mat4_to_buffer(buffer, data.model_m[i]);
    }
}

void read_Grass_from_buffer(DeserializerBuffer& buffer, Grass& data) {
    read_model_handle_from_buffer(buffer, data.placement_model);
    read_n_from_buffer(buffer, &data.cast_shadows, sizeof(bool));
    read_n_from_buffer(buffer, &data.width, sizeof(float));
    read_n_from_buffer(buffer, &data.height, sizeof(float));
    read_n_from_buffer(buffer, &data.max_height, sizeof(float));
    read_n_from_buffer(buffer, &data.density, sizeof(float));
    read_n_from_buffer(buffer, &data.random_rotation, sizeof(float));
    read_n_from_buffer(buffer, &data.random_scale, sizeof(float));
    read_n_from_buffer(buffer, &data.align_to_terrain_normal, sizeof(bool));
    data.positions.resize(read_uint_from_buffer(buffer));
	for (uint i = 0; i < data.positions.length; i++) {
         read_vec3_from_buffer(buffer, data.positions[i]);
    }
    data.model_m.resize(read_uint_from_buffer(buffer));
	for (uint i = 0; i < data.model_m.length; i++) {
         read_mat4_from_buffer(buffer, data.model_m[i]);
    }
}

refl::Struct* get_Grass_type() {
	static refl::Struct type = init_Grass();
	return &type;
}

refl::Struct init_model_handle() {
	refl::Struct type("model_handle", sizeof(model_handle));
	type.fields.append({"id", offsetof(model_handle, id), get_uint_type()});
	return type;
}

void write_model_handle_to_buffer(SerializerBuffer& buffer, model_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(model_handle));
}

void read_model_handle_from_buffer(DeserializerBuffer& buffer, model_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(model_handle));
}

refl::Struct* get_model_handle_type() {
	static refl::Struct type = init_model_handle();
	return &type;
}

refl::Struct init_texture_handle() {
	refl::Struct type("texture_handle", sizeof(texture_handle));
	type.fields.append({"id", offsetof(texture_handle, id), get_uint_type()});
	return type;
}

void write_texture_handle_to_buffer(SerializerBuffer& buffer, texture_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(texture_handle));
}

void read_texture_handle_from_buffer(DeserializerBuffer& buffer, texture_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(texture_handle));
}

refl::Struct* get_texture_handle_type() {
	static refl::Struct type = init_texture_handle();
	return &type;
}

refl::Struct init_shader_handle() {
	refl::Struct type("shader_handle", sizeof(shader_handle));
	type.fields.append({"id", offsetof(shader_handle, id), get_uint_type()});
	return type;
}

void write_shader_handle_to_buffer(SerializerBuffer& buffer, shader_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(shader_handle));
}

void read_shader_handle_from_buffer(DeserializerBuffer& buffer, shader_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(shader_handle));
}

refl::Struct* get_shader_handle_type() {
	static refl::Struct type = init_shader_handle();
	return &type;
}

refl::Struct init_cubemap_handle() {
	refl::Struct type("cubemap_handle", sizeof(cubemap_handle));
	type.fields.append({"id", offsetof(cubemap_handle, id), get_uint_type()});
	return type;
}

void write_cubemap_handle_to_buffer(SerializerBuffer& buffer, cubemap_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(cubemap_handle));
}

void read_cubemap_handle_from_buffer(DeserializerBuffer& buffer, cubemap_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(cubemap_handle));
}

refl::Struct* get_cubemap_handle_type() {
	static refl::Struct type = init_cubemap_handle();
	return &type;
}

refl::Struct init_material_handle() {
	refl::Struct type("material_handle", sizeof(material_handle));
	type.fields.append({"id", offsetof(material_handle, id), get_uint_type()});
	return type;
}

void write_material_handle_to_buffer(SerializerBuffer& buffer, material_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(material_handle));
}

void read_material_handle_from_buffer(DeserializerBuffer& buffer, material_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(material_handle));
}

refl::Struct* get_material_handle_type() {
	static refl::Struct type = init_material_handle();
	return &type;
}

refl::Struct init_env_probe_handle() {
	refl::Struct type("env_probe_handle", sizeof(env_probe_handle));
	type.fields.append({"id", offsetof(env_probe_handle, id), get_uint_type()});
	return type;
}

void write_env_probe_handle_to_buffer(SerializerBuffer& buffer, env_probe_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(env_probe_handle));
}

void read_env_probe_handle_from_buffer(DeserializerBuffer& buffer, env_probe_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(env_probe_handle));
}

refl::Struct* get_env_probe_handle_type() {
	static refl::Struct type = init_env_probe_handle();
	return &type;
}

refl::Struct init_shader_config_handle() {
	refl::Struct type("shader_config_handle", sizeof(shader_config_handle));
	type.fields.append({"id", offsetof(shader_config_handle, id), get_uint_type()});
	return type;
}

void write_shader_config_handle_to_buffer(SerializerBuffer& buffer, shader_config_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(shader_config_handle));
}

void read_shader_config_handle_from_buffer(DeserializerBuffer& buffer, shader_config_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(shader_config_handle));
}

refl::Struct* get_shader_config_handle_type() {
	static refl::Struct type = init_shader_config_handle();
	return &type;
}

refl::Struct init_uniform_handle() {
	refl::Struct type("uniform_handle", sizeof(uniform_handle));
	type.fields.append({"id", offsetof(uniform_handle, id), get_uint_type()});
	return type;
}

void write_uniform_handle_to_buffer(SerializerBuffer& buffer, uniform_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(uniform_handle));
}

void read_uniform_handle_from_buffer(DeserializerBuffer& buffer, uniform_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(uniform_handle));
}

refl::Struct* get_uniform_handle_type() {
	static refl::Struct type = init_uniform_handle();
	return &type;
}

refl::Struct init_pipeline_handle() {
	refl::Struct type("pipeline_handle", sizeof(pipeline_handle));
	type.fields.append({"id", offsetof(pipeline_handle, id), get_uint_type()});
	return type;
}

void write_pipeline_handle_to_buffer(SerializerBuffer& buffer, pipeline_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(pipeline_handle));
}

void read_pipeline_handle_from_buffer(DeserializerBuffer& buffer, pipeline_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(pipeline_handle));
}

refl::Struct* get_pipeline_handle_type() {
	static refl::Struct type = init_pipeline_handle();
	return &type;
}

refl::Struct init_pipeline_layout_handle() {
	refl::Struct type("pipeline_layout_handle", sizeof(pipeline_layout_handle));
	type.fields.append({"id", offsetof(pipeline_layout_handle, id), get_u64_type()});
	return type;
}

void write_pipeline_layout_handle_to_buffer(SerializerBuffer& buffer, pipeline_layout_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(pipeline_layout_handle));
}

void read_pipeline_layout_handle_from_buffer(DeserializerBuffer& buffer, pipeline_layout_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(pipeline_layout_handle));
}

refl::Struct* get_pipeline_layout_handle_type() {
	static refl::Struct type = init_pipeline_layout_handle();
	return &type;
}

refl::Struct init_descriptor_set_handle() {
	refl::Struct type("descriptor_set_handle", sizeof(descriptor_set_handle));
	type.fields.append({"id", offsetof(descriptor_set_handle, id), get_uint_type()});
	return type;
}

void write_descriptor_set_handle_to_buffer(SerializerBuffer& buffer, descriptor_set_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(descriptor_set_handle));
}

void read_descriptor_set_handle_from_buffer(DeserializerBuffer& buffer, descriptor_set_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(descriptor_set_handle));
}

refl::Struct* get_descriptor_set_handle_type() {
	static refl::Struct type = init_descriptor_set_handle();
	return &type;
}

refl::Struct init_render_pass_handle() {
	refl::Struct type("render_pass_handle", sizeof(render_pass_handle));
	type.fields.append({"id", offsetof(render_pass_handle, id), get_u64_type()});
	return type;
}

void write_render_pass_handle_to_buffer(SerializerBuffer& buffer, render_pass_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(render_pass_handle));
}

void read_render_pass_handle_from_buffer(DeserializerBuffer& buffer, render_pass_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(render_pass_handle));
}

refl::Struct* get_render_pass_handle_type() {
	static refl::Struct type = init_render_pass_handle();
	return &type;
}

refl::Struct init_command_buffer_handle() {
	refl::Struct type("command_buffer_handle", sizeof(command_buffer_handle));
	type.fields.append({"id", offsetof(command_buffer_handle, id), get_u64_type()});
	return type;
}

void write_command_buffer_handle_to_buffer(SerializerBuffer& buffer, command_buffer_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(command_buffer_handle));
}

void read_command_buffer_handle_from_buffer(DeserializerBuffer& buffer, command_buffer_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(command_buffer_handle));
}

refl::Struct* get_command_buffer_handle_type() {
	static refl::Struct type = init_command_buffer_handle();
	return &type;
}

refl::Struct init_frame_buffer_handle() {
	refl::Struct type("frame_buffer_handle", sizeof(frame_buffer_handle));
	type.fields.append({"id", offsetof(frame_buffer_handle, id), get_u64_type()});
	return type;
}

void write_frame_buffer_handle_to_buffer(SerializerBuffer& buffer, frame_buffer_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(frame_buffer_handle));
}

void read_frame_buffer_handle_from_buffer(DeserializerBuffer& buffer, frame_buffer_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(frame_buffer_handle));
}

refl::Struct* get_frame_buffer_handle_type() {
	static refl::Struct type = init_frame_buffer_handle();
	return &type;
}

refl::Struct init_CapsuleCollider() {
	refl::Struct type("CapsuleCollider", sizeof(CapsuleCollider));
	type.fields.append({"radius", offsetof(CapsuleCollider, radius), get_float_type()});
	type.fields.append({"height", offsetof(CapsuleCollider, height), get_float_type()});
	return type;
}

void write_CapsuleCollider_to_buffer(SerializerBuffer& buffer, CapsuleCollider& data) {
    write_n_to_buffer(buffer, &data, sizeof(CapsuleCollider));
}

void read_CapsuleCollider_from_buffer(DeserializerBuffer& buffer, CapsuleCollider& data) {
    read_n_from_buffer(buffer, &data, sizeof(CapsuleCollider));
}

refl::Struct* get_CapsuleCollider_type() {
	static refl::Struct type = init_CapsuleCollider();
	return &type;
}

refl::Struct init_SphereCollider() {
	refl::Struct type("SphereCollider", sizeof(SphereCollider));
	type.fields.append({"radius", offsetof(SphereCollider, radius), get_float_type()});
	return type;
}

void write_SphereCollider_to_buffer(SerializerBuffer& buffer, SphereCollider& data) {
    write_n_to_buffer(buffer, &data, sizeof(SphereCollider));
}

void read_SphereCollider_from_buffer(DeserializerBuffer& buffer, SphereCollider& data) {
    read_n_from_buffer(buffer, &data, sizeof(SphereCollider));
}

refl::Struct* get_SphereCollider_type() {
	static refl::Struct type = init_SphereCollider();
	return &type;
}

refl::Struct init_BoxCollider() {
	refl::Struct type("BoxCollider", sizeof(BoxCollider));
	type.fields.append({"scale", offsetof(BoxCollider, scale), get_vec3_type()});
	return type;
}

void write_BoxCollider_to_buffer(SerializerBuffer& buffer, BoxCollider& data) {
    write_n_to_buffer(buffer, &data, sizeof(BoxCollider));
}

void read_BoxCollider_from_buffer(DeserializerBuffer& buffer, BoxCollider& data) {
    read_n_from_buffer(buffer, &data, sizeof(BoxCollider));
}

refl::Struct* get_BoxCollider_type() {
	static refl::Struct type = init_BoxCollider();
	return &type;
}

refl::Struct init_PlaneCollider() {
	refl::Struct type("PlaneCollider", sizeof(PlaneCollider));
	type.fields.append({"normal", offsetof(PlaneCollider, normal), get_vec3_type()});
	return type;
}

void write_PlaneCollider_to_buffer(SerializerBuffer& buffer, PlaneCollider& data) {
    write_n_to_buffer(buffer, &data, sizeof(PlaneCollider));
}

void read_PlaneCollider_from_buffer(DeserializerBuffer& buffer, PlaneCollider& data) {
    read_n_from_buffer(buffer, &data, sizeof(PlaneCollider));
}

refl::Struct* get_PlaneCollider_type() {
	static refl::Struct type = init_PlaneCollider();
	return &type;
}

refl::Struct init_RigidBody() {
	refl::Struct type("RigidBody", sizeof(RigidBody));
	type.fields.append({"mass", offsetof(RigidBody, mass), get_float_type()});
	type.fields.append({"velocity", offsetof(RigidBody, velocity), get_vec3_type()});
	type.fields.append({"override_position", offsetof(RigidBody, override_position), get_bool_type()});
	type.fields.append({"override_rotation", offsetof(RigidBody, override_rotation), get_bool_type()});
	type.fields.append({"override_velocity_x", offsetof(RigidBody, override_velocity_x), get_bool_type()});
	type.fields.append({"override_velocity_y", offsetof(RigidBody, override_velocity_y), get_bool_type()});
	type.fields.append({"override_velocity_z", offsetof(RigidBody, override_velocity_z), get_bool_type()});
	type.fields.append({"continous", offsetof(RigidBody, continous), get_bool_type()});
	return type;
}

void write_RigidBody_to_buffer(SerializerBuffer& buffer, RigidBody& data) {
    write_n_to_buffer(buffer, &data, sizeof(RigidBody));
}

void read_RigidBody_from_buffer(DeserializerBuffer& buffer, RigidBody& data) {
    read_n_from_buffer(buffer, &data, sizeof(RigidBody));
    data.bt_rigid_body = nullptr;
}

refl::Struct* get_RigidBody_type() {
	static refl::Struct type = init_RigidBody();
	return &type;
}

refl::Struct init_CharacterController() {
	refl::Struct type("CharacterController", sizeof(CharacterController));
	type.fields.append({"on_ground", offsetof(CharacterController, on_ground), get_bool_type()});
	type.fields.append({"velocity", offsetof(CharacterController, velocity), get_vec3_type()});
	type.fields.append({"feet_height", offsetof(CharacterController, feet_height), get_float_type()});
	return type;
}

void write_CharacterController_to_buffer(SerializerBuffer& buffer, CharacterController& data) {
    write_n_to_buffer(buffer, &data, sizeof(CharacterController));
}

void read_CharacterController_from_buffer(DeserializerBuffer& buffer, CharacterController& data) {
    read_n_from_buffer(buffer, &data, sizeof(CharacterController));
}

refl::Struct* get_CharacterController_type() {
	static refl::Struct type = init_CharacterController();
	return &type;
}

refl::Struct init_ParamDesc() {
	refl::Struct type("ParamDesc", sizeof(ParamDesc));
	type.fields.append({"type", offsetof(ParamDesc, type), get_Param_Type_type()});
	type.fields.append({"name", offsetof(ParamDesc, name), get_sstring_type()});
	type.fields.append({"image", offsetof(ParamDesc, image), get_uint_type()});
    static refl::Union inline_union = { refl::Type::Union, 0, "ParamDesc" };
    type.fields.append({ "", offsetof(ParamDesc, type), &inline_union });
	return type;
}

void write_ParamDesc_to_buffer(SerializerBuffer& buffer, ParamDesc& data) {
    write_n_to_buffer(buffer, &data, sizeof(ParamDesc));
}

void read_ParamDesc_from_buffer(DeserializerBuffer& buffer, ParamDesc& data) {
    read_n_from_buffer(buffer, &data, sizeof(ParamDesc));
}

refl::Struct* get_ParamDesc_type() {
	static refl::Struct type = init_ParamDesc();
	return &type;
}

refl::Struct init_MaterialDesc() {
	refl::Struct type("MaterialDesc", sizeof(MaterialDesc));
	type.fields.append({"shader", offsetof(MaterialDesc, shader), get_shader_handle_type()});
	type.fields.append({"mode", offsetof(MaterialDesc, mode), get_MaterialDesc_Mode_type()});
	type.fields.append({"draw_state", offsetof(MaterialDesc, draw_state), get_DrawCommandState_type()});
	type.fields.append({"params", offsetof(MaterialDesc, params), make_array_type(10, sizeof(array<10, struct ParamDesc>), get_ParamDesc_type())});
	type.fields.append({"flags", offsetof(MaterialDesc, flags), get_uint_type()});
	return type;
}

void write_MaterialDesc_to_buffer(SerializerBuffer& buffer, MaterialDesc& data) {
    write_n_to_buffer(buffer, &data, sizeof(MaterialDesc));
}

void read_MaterialDesc_from_buffer(DeserializerBuffer& buffer, MaterialDesc& data) {
    read_n_from_buffer(buffer, &data, sizeof(MaterialDesc));
}

refl::Struct* get_MaterialDesc_type() {
	static refl::Struct type = init_MaterialDesc();
	return &type;
}

refl::Struct init_Materials() {
	refl::Struct type("Materials", sizeof(Materials));
	type.fields.append({"materials", offsetof(Materials, materials), make_array_type(8, sizeof(array<8, struct material_handle>), get_material_handle_type())});
	return type;
}

void write_Materials_to_buffer(SerializerBuffer& buffer, Materials& data) {
    write_n_to_buffer(buffer, &data, sizeof(Materials));
}

void read_Materials_from_buffer(DeserializerBuffer& buffer, Materials& data) {
    read_n_from_buffer(buffer, &data, sizeof(Materials));
}

refl::Struct* get_Materials_type() {
	static refl::Struct type = init_Materials();
	return &type;
}

refl::Struct init_ModelRenderer() {
	refl::Struct type("ModelRenderer", sizeof(ModelRenderer));
	type.fields.append({"visible", offsetof(ModelRenderer, visible), get_bool_type()});
	type.fields.append({"model_id", offsetof(ModelRenderer, model_id), get_model_handle_type()});
	return type;
}

void write_ModelRenderer_to_buffer(SerializerBuffer& buffer, ModelRenderer& data) {
    write_n_to_buffer(buffer, &data, sizeof(ModelRenderer));
}

void read_ModelRenderer_from_buffer(DeserializerBuffer& buffer, ModelRenderer& data) {
    read_n_from_buffer(buffer, &data, sizeof(ModelRenderer));
}

refl::Struct* get_ModelRenderer_type() {
	static refl::Struct type = init_ModelRenderer();
	return &type;
}

refl::Struct init_ShaderInfo() {
	refl::Struct type("ShaderInfo", sizeof(ShaderInfo));
	type.fields.append({"vfilename", offsetof(ShaderInfo, vfilename), get_sstring_type()});
	type.fields.append({"ffilename", offsetof(ShaderInfo, ffilename), get_sstring_type()});
	type.fields.append({"v_time_modified", offsetof(ShaderInfo, v_time_modified), get_u64_type()});
	type.fields.append({"f_time_modified", offsetof(ShaderInfo, f_time_modified), get_u64_type()});
	return type;
}

void write_ShaderInfo_to_buffer(SerializerBuffer& buffer, ShaderInfo& data) {
    write_n_to_buffer(buffer, &data, sizeof(ShaderInfo));
}

void read_ShaderInfo_from_buffer(DeserializerBuffer& buffer, ShaderInfo& data) {
    read_n_from_buffer(buffer, &data, sizeof(ShaderInfo));
}

refl::Struct* get_ShaderInfo_type() {
	static refl::Struct type = init_ShaderInfo();
	return &type;
}

refl::Struct init_Entity() {
	refl::Struct type("Entity", sizeof(Entity));
	type.fields.append({"id", offsetof(Entity, id), get_ID_type()});
	return type;
}

void write_Entity_to_buffer(SerializerBuffer& buffer, Entity& data) {
    write_n_to_buffer(buffer, &data, sizeof(Entity));
}

void read_Entity_from_buffer(DeserializerBuffer& buffer, Entity& data) {
    read_n_from_buffer(buffer, &data, sizeof(Entity));
}

refl::Struct* get_Entity_type() {
	static refl::Struct type = init_Entity();
	return &type;
}

refl::Struct init_ArchetypeStore() {
	refl::Struct type("ArchetypeStore", sizeof(ArchetypeStore));
	type.fields.append({"offsets", offsetof(ArchetypeStore, offsets), make_carray_type(64, get_uint_type())});
	type.fields.append({"dirty_hierarchy_offset", offsetof(ArchetypeStore, dirty_hierarchy_offset), make_carray_type(3, get_uint_type())});
	type.fields.append({"dirty_hierarchy_count", offsetof(ArchetypeStore, dirty_hierarchy_count), make_carray_type(3, get_uint_type())});
	type.fields.append({"block_count", offsetof(ArchetypeStore, block_count), get_uint_type()});
	type.fields.append({"max_per_block", offsetof(ArchetypeStore, max_per_block), get_uint_type()});
	type.fields.append({"entity_count_last_block", offsetof(ArchetypeStore, entity_count_last_block), get_uint_type()});
	return type;
}

void write_ArchetypeStore_to_buffer(SerializerBuffer& buffer, ArchetypeStore& data) {
    write_n_to_buffer(buffer, &data, sizeof(ArchetypeStore));
}

void read_ArchetypeStore_from_buffer(DeserializerBuffer& buffer, ArchetypeStore& data) {
    read_n_from_buffer(buffer, &data, sizeof(ArchetypeStore));
}

refl::Struct* get_ArchetypeStore_type() {
	static refl::Struct type = init_ArchetypeStore();
	return &type;
}

#include "ecs/component_ids.h"
#include "ecs/ecs.h"
#include "engine/application.h"


void register_default_components(World& world) {
    world.component_type[1] = get_Transform_type();
    world.component_size[1] = sizeof(Transform);
    world.component_lifetime_funcs[1].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Transform*)data + i) Transform(); };
    world.component_type[2] = get_StaticTransform_type();
    world.component_size[2] = sizeof(StaticTransform);
    world.component_lifetime_funcs[2].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((StaticTransform*)data + i) StaticTransform(); };
    world.component_type[3] = get_LocalTransform_type();
    world.component_size[3] = sizeof(LocalTransform);
    world.component_lifetime_funcs[3].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((LocalTransform*)data + i) LocalTransform(); };
    world.component_type[4] = get_Camera_type();
    world.component_size[4] = sizeof(Camera);
    world.component_lifetime_funcs[4].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Camera*)data + i) Camera(); };
    world.component_type[5] = get_Flyover_type();
    world.component_size[5] = sizeof(Flyover);
    world.component_lifetime_funcs[5].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Flyover*)data + i) Flyover(); };
    world.component_type[6] = get_Skybox_type();
    world.component_size[6] = sizeof(Skybox);
    world.component_lifetime_funcs[6].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Skybox*)data + i) Skybox(); };
    world.component_type[7] = get_TerrainControlPoint_type();
    world.component_size[7] = sizeof(TerrainControlPoint);
    world.component_lifetime_funcs[7].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((TerrainControlPoint*)data + i) TerrainControlPoint(); };
    world.component_type[8] = get_TerrainSplat_type();
    world.component_size[8] = sizeof(TerrainSplat);
    world.component_lifetime_funcs[8].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((TerrainSplat*)data + i) TerrainSplat(); };
    world.component_type[9] = get_Terrain_type();
    world.component_size[9] = sizeof(Terrain);
    world.component_lifetime_funcs[9].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Terrain*)data + i) Terrain(); };
    world.component_lifetime_funcs[9].copy = [](void* dst, void* src, uint count) { for (uint i=0; i<count; i++) new ((Terrain*)dst + i) Terrain(((Terrain*)src)[i]); };
    world.component_lifetime_funcs[9].destructor = [](void* ptr, uint count) { for (uint i=0; i<count; i++) ((Terrain*)ptr)[i].~Terrain(); };
    world.component_lifetime_funcs[9].serialize = [](SerializerBuffer& buffer, void* data, uint count) {
        for (uint i = 0; i < count; i++) write_Terrain_to_buffer(buffer, ((Terrain*)data)[i]);
    };
    world.component_lifetime_funcs[9].deserialize = [](DeserializerBuffer& buffer, void* data, uint count) {
        for (uint i = 0; i < count; i++) read_Terrain_from_buffer(buffer, ((Terrain*)data)[i]);
    };


    world.component_type[10] = get_DirLight_type();
    world.component_size[10] = sizeof(DirLight);
    world.component_lifetime_funcs[10].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((DirLight*)data + i) DirLight(); };
    world.component_type[11] = get_PointLight_type();
    world.component_size[11] = sizeof(PointLight);
    world.component_lifetime_funcs[11].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((PointLight*)data + i) PointLight(); };
    world.component_type[12] = get_SkyLight_type();
    world.component_size[12] = sizeof(SkyLight);
    world.component_lifetime_funcs[12].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((SkyLight*)data + i) SkyLight(); };
    world.component_type[13] = get_Grass_type();
    world.component_size[13] = sizeof(Grass);
    world.component_lifetime_funcs[13].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Grass*)data + i) Grass(); };
    world.component_lifetime_funcs[13].copy = [](void* dst, void* src, uint count) { for (uint i=0; i<count; i++) new ((Grass*)dst + i) Grass(((Grass*)src)[i]); };
    world.component_lifetime_funcs[13].destructor = [](void* ptr, uint count) { for (uint i=0; i<count; i++) ((Grass*)ptr)[i].~Grass(); };
    world.component_lifetime_funcs[13].serialize = [](SerializerBuffer& buffer, void* data, uint count) {
        for (uint i = 0; i < count; i++) write_Grass_to_buffer(buffer, ((Grass*)data)[i]);
    };
    world.component_lifetime_funcs[13].deserialize = [](DeserializerBuffer& buffer, void* data, uint count) {
        for (uint i = 0; i < count; i++) read_Grass_from_buffer(buffer, ((Grass*)data)[i]);
    };


    world.component_type[14] = get_CapsuleCollider_type();
    world.component_size[14] = sizeof(CapsuleCollider);
    world.component_lifetime_funcs[14].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((CapsuleCollider*)data + i) CapsuleCollider(); };
    world.component_type[15] = get_SphereCollider_type();
    world.component_size[15] = sizeof(SphereCollider);
    world.component_lifetime_funcs[15].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((SphereCollider*)data + i) SphereCollider(); };
    world.component_type[16] = get_BoxCollider_type();
    world.component_size[16] = sizeof(BoxCollider);
    world.component_lifetime_funcs[16].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((BoxCollider*)data + i) BoxCollider(); };
    world.component_type[17] = get_PlaneCollider_type();
    world.component_size[17] = sizeof(PlaneCollider);
    world.component_lifetime_funcs[17].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((PlaneCollider*)data + i) PlaneCollider(); };
    world.component_type[18] = get_RigidBody_type();
    world.component_size[18] = sizeof(RigidBody);
    world.component_lifetime_funcs[18].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((RigidBody*)data + i) RigidBody(); };
    world.component_type[19] = get_CharacterController_type();
    world.component_size[19] = sizeof(CharacterController);
    world.component_lifetime_funcs[19].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((CharacterController*)data + i) CharacterController(); };
    world.component_type[20] = get_Materials_type();
    world.component_size[20] = sizeof(Materials);
    world.component_lifetime_funcs[20].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Materials*)data + i) Materials(); };
    world.component_type[21] = get_ModelRenderer_type();
    world.component_size[21] = sizeof(ModelRenderer);
    world.component_lifetime_funcs[21].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((ModelRenderer*)data + i) ModelRenderer(); };
    world.component_type[0] = get_Entity_type();
    world.component_size[0] = sizeof(Entity);
    world.component_lifetime_funcs[0].constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Entity*)data + i) Entity(); };

};
