#include "../src/generated.h"
#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"
#include "core/handle.h"
#include "components/camera.h"
#include "components/flyover.h"
#include "components/grass.h"
#include "components/lights.h"
#include "components/skybox.h"
#include "components/terrain.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "physics/physics.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/assets_store.h"
#include "graphics/assets/material.h"
#include "graphics/assets/model.h"
#include "graphics/assets/shader.h"
#include "graphics/assets/texture.h"

refl::Enum init_MaterialDesc_Mode() {
	refl::Enum type("Mode", sizeof(MaterialDesc::Mode));
	type.values.append({ "Static", MaterialDesc::Mode::Static });
	return type;
}

ENGINE_API refl::Type* refl::TypeResolver<MaterialDesc::Mode>::get() {
	static refl::Enum type = init_MaterialDesc_Mode();
	return &type;
}
refl::Struct init_model_handle() {
	refl::Struct type("model_handle", sizeof(model_handle));
	type.fields.append({"id", offsetof(model_handle, id), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, model_handle& data) {
    write_to_buffer(buffer, data.id);
}

ENGINE_API refl::Type* refl::TypeResolver<model_handle>::get() {
	static refl::Struct type = init_model_handle();
	return &type;
}
refl::Struct init_texture_handle() {
	refl::Struct type("texture_handle", sizeof(texture_handle));
	type.fields.append({"id", offsetof(texture_handle, id), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, texture_handle& data) {
    write_to_buffer(buffer, data.id);
}

ENGINE_API refl::Type* refl::TypeResolver<texture_handle>::get() {
	static refl::Struct type = init_texture_handle();
	return &type;
}
refl::Struct init_shader_handle() {
	refl::Struct type("shader_handle", sizeof(shader_handle));
	type.fields.append({"id", offsetof(shader_handle, id), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, shader_handle& data) {
    write_to_buffer(buffer, data.id);
}

ENGINE_API refl::Type* refl::TypeResolver<shader_handle>::get() {
	static refl::Struct type = init_shader_handle();
	return &type;
}
refl::Struct init_cubemap_handle() {
	refl::Struct type("cubemap_handle", sizeof(cubemap_handle));
	type.fields.append({"id", offsetof(cubemap_handle, id), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, cubemap_handle& data) {
    write_to_buffer(buffer, data.id);
}

ENGINE_API refl::Type* refl::TypeResolver<cubemap_handle>::get() {
	static refl::Struct type = init_cubemap_handle();
	return &type;
}
refl::Struct init_material_handle() {
	refl::Struct type("material_handle", sizeof(material_handle));
	type.fields.append({"id", offsetof(material_handle, id), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, material_handle& data) {
    write_to_buffer(buffer, data.id);
}

ENGINE_API refl::Type* refl::TypeResolver<material_handle>::get() {
	static refl::Struct type = init_material_handle();
	return &type;
}
refl::Struct init_env_probe_handle() {
	refl::Struct type("env_probe_handle", sizeof(env_probe_handle));
	type.fields.append({"id", offsetof(env_probe_handle, id), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, env_probe_handle& data) {
    write_to_buffer(buffer, data.id);
}

ENGINE_API refl::Type* refl::TypeResolver<env_probe_handle>::get() {
	static refl::Struct type = init_env_probe_handle();
	return &type;
}
refl::Struct init_shader_config_handle() {
	refl::Struct type("shader_config_handle", sizeof(shader_config_handle));
	type.fields.append({"id", offsetof(shader_config_handle, id), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, shader_config_handle& data) {
    write_to_buffer(buffer, data.id);
}

ENGINE_API refl::Type* refl::TypeResolver<shader_config_handle>::get() {
	static refl::Struct type = init_shader_config_handle();
	return &type;
}
refl::Struct init_uniform_handle() {
	refl::Struct type("uniform_handle", sizeof(uniform_handle));
	type.fields.append({"id", offsetof(uniform_handle, id), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, uniform_handle& data) {
    write_to_buffer(buffer, data.id);
}

ENGINE_API refl::Type* refl::TypeResolver<uniform_handle>::get() {
	static refl::Struct type = init_uniform_handle();
	return &type;
}
refl::Struct init_pipeline_handle() {
	refl::Struct type("pipeline_handle", sizeof(pipeline_handle));
	type.fields.append({"id", offsetof(pipeline_handle, id), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, pipeline_handle& data) {
    write_to_buffer(buffer, data.id);
}

ENGINE_API refl::Type* refl::TypeResolver<pipeline_handle>::get() {
	static refl::Struct type = init_pipeline_handle();
	return &type;
}
refl::Struct init_pipeline_layout_handle() {
	refl::Struct type("pipeline_layout_handle", sizeof(pipeline_layout_handle));
	type.fields.append({"id", offsetof(pipeline_layout_handle, id), refl::TypeResolver<u64>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, pipeline_layout_handle& data) {
}

ENGINE_API refl::Type* refl::TypeResolver<pipeline_layout_handle>::get() {
	static refl::Struct type = init_pipeline_layout_handle();
	return &type;
}
refl::Struct init_descriptor_set_handle() {
	refl::Struct type("descriptor_set_handle", sizeof(descriptor_set_handle));
	type.fields.append({"id", offsetof(descriptor_set_handle, id), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, descriptor_set_handle& data) {
    write_to_buffer(buffer, data.id);
}

ENGINE_API refl::Type* refl::TypeResolver<descriptor_set_handle>::get() {
	static refl::Struct type = init_descriptor_set_handle();
	return &type;
}
refl::Struct init_render_pass_handle() {
	refl::Struct type("render_pass_handle", sizeof(render_pass_handle));
	type.fields.append({"id", offsetof(render_pass_handle, id), refl::TypeResolver<u64>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, render_pass_handle& data) {
}

ENGINE_API refl::Type* refl::TypeResolver<render_pass_handle>::get() {
	static refl::Struct type = init_render_pass_handle();
	return &type;
}
refl::Struct init_command_buffer_handle() {
	refl::Struct type("command_buffer_handle", sizeof(command_buffer_handle));
	type.fields.append({"id", offsetof(command_buffer_handle, id), refl::TypeResolver<u64>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, command_buffer_handle& data) {
}

ENGINE_API refl::Type* refl::TypeResolver<command_buffer_handle>::get() {
	static refl::Struct type = init_command_buffer_handle();
	return &type;
}
refl::Struct init_frame_buffer_handle() {
	refl::Struct type("frame_buffer_handle", sizeof(frame_buffer_handle));
	type.fields.append({"id", offsetof(frame_buffer_handle, id), refl::TypeResolver<u64>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, frame_buffer_handle& data) {
}

ENGINE_API refl::Type* refl::TypeResolver<frame_buffer_handle>::get() {
	static refl::Struct type = init_frame_buffer_handle();
	return &type;
}
refl::Struct init_Camera() {
	refl::Struct type("Camera", sizeof(Camera));
	type.fields.append({"near_plane", offsetof(Camera, near_plane), refl::TypeResolver<float>::get() });
	type.fields.append({"far_plane", offsetof(Camera, far_plane), refl::TypeResolver<float>::get() });
	type.fields.append({"fov", offsetof(Camera, fov), refl::TypeResolver<float>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Camera& data) {
    write_to_buffer(buffer, data.near_plane);
    write_to_buffer(buffer, data.far_plane);
    write_to_buffer(buffer, data.fov);
}

ENGINE_API refl::Type* refl::TypeResolver<Camera>::get() {
	static refl::Struct type = init_Camera();
	return &type;
}
refl::Struct init_Flyover() {
	refl::Struct type("Flyover", sizeof(Flyover));
	type.fields.append({"mouse_sensitivity", offsetof(Flyover, mouse_sensitivity), refl::TypeResolver<float>::get() });
	type.fields.append({"movement_speed", offsetof(Flyover, movement_speed), refl::TypeResolver<float>::get() });
	type.fields.append({"yaw", offsetof(Flyover, yaw), refl::TypeResolver<float>::get() });
	type.fields.append({"pitch", offsetof(Flyover, pitch), refl::TypeResolver<float>::get() });
	type.fields.append({"past_movement_speed", offsetof(Flyover, past_movement_speed), refl::TypeResolver<glm::vec2[3]>::get() });
	type.fields.append({"past_movement_speed_length", offsetof(Flyover, past_movement_speed_length), refl::TypeResolver<int>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Flyover& data) {
    write_to_buffer(buffer, data.mouse_sensitivity);
    write_to_buffer(buffer, data.movement_speed);
    write_to_buffer(buffer, data.yaw);
    write_to_buffer(buffer, data.pitch);
	for (uint i = 0; i < 3; i++) {
         write_to_buffer(buffer, data.past_movement_speed[i]);
    }
    write_to_buffer(buffer, data.past_movement_speed_length);
}

ENGINE_API refl::Type* refl::TypeResolver<Flyover>::get() {
	static refl::Struct type = init_Flyover();
	return &type;
}
refl::Struct init_Grass() {
	refl::Struct type("Grass", sizeof(Grass));
	type.fields.append({"placement_model", offsetof(Grass, placement_model), refl::TypeResolver<model_handle>::get() });
	type.fields.append({"cast_shadows", offsetof(Grass, cast_shadows), refl::TypeResolver<bool>::get() });
	type.fields.append({"width", offsetof(Grass, width), refl::TypeResolver<float>::get() });
	type.fields.append({"height", offsetof(Grass, height), refl::TypeResolver<float>::get() });
	type.fields.append({"max_height", offsetof(Grass, max_height), refl::TypeResolver<float>::get() });
	type.fields.append({"density", offsetof(Grass, density), refl::TypeResolver<float>::get() });
	type.fields.append({"random_rotation", offsetof(Grass, random_rotation), refl::TypeResolver<float>::get() });
	type.fields.append({"random_scale", offsetof(Grass, random_scale), refl::TypeResolver<float>::get() });
	type.fields.append({"align_to_terrain_normal", offsetof(Grass, align_to_terrain_normal), refl::TypeResolver<bool>::get() });
	type.fields.append({"transforms", offsetof(Grass, transforms), refl::TypeResolver<vector<Transform>>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Grass& data) {
    write_to_buffer(buffer, data.placement_model);
    write_to_buffer(buffer, data.cast_shadows);
    write_to_buffer(buffer, data.width);
    write_to_buffer(buffer, data.height);
    write_to_buffer(buffer, data.max_height);
    write_to_buffer(buffer, data.density);
    write_to_buffer(buffer, data.random_rotation);
    write_to_buffer(buffer, data.random_scale);
    write_to_buffer(buffer, data.align_to_terrain_normal);
	for (uint i = 0; i < data.transforms.length; i++) {
         write_to_buffer(buffer, data.transforms[i]);
    }
}

ENGINE_API refl::Type* refl::TypeResolver<Grass>::get() {
	static refl::Struct type = init_Grass();
	return &type;
}
refl::Struct init_DirLight() {
	refl::Struct type("DirLight", sizeof(DirLight));
	type.fields.append({"direction", offsetof(DirLight, direction), refl::TypeResolver<glm::vec3>::get() });
	type.fields.append({"color", offsetof(DirLight, color), refl::TypeResolver<glm::vec3>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, DirLight& data) {
    write_to_buffer(buffer, data.direction);
    write_to_buffer(buffer, data.color);
}

ENGINE_API refl::Type* refl::TypeResolver<DirLight>::get() {
	static refl::Struct type = init_DirLight();
	return &type;
}
refl::Struct init_PointLight() {
	refl::Struct type("PointLight", sizeof(PointLight));
	type.fields.append({"color", offsetof(PointLight, color), refl::TypeResolver<glm::vec3>::get() });
	type.fields.append({"radius", offsetof(PointLight, radius), refl::TypeResolver<float>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, PointLight& data) {
    write_to_buffer(buffer, data.color);
    write_to_buffer(buffer, data.radius);
}

ENGINE_API refl::Type* refl::TypeResolver<PointLight>::get() {
	static refl::Struct type = init_PointLight();
	return &type;
}
refl::Struct init_SkyLight() {
	refl::Struct type("SkyLight", sizeof(SkyLight));
	type.fields.append({"capture_scene", offsetof(SkyLight, capture_scene), refl::TypeResolver<bool>::get() });
	type.fields.append({"cubemap", offsetof(SkyLight, cubemap), refl::TypeResolver<cubemap_handle>::get() });
	type.fields.append({"irradiance", offsetof(SkyLight, irradiance), refl::TypeResolver<cubemap_handle>::get() });
	type.fields.append({"prefilter", offsetof(SkyLight, prefilter), refl::TypeResolver<cubemap_handle>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, SkyLight& data) {
    write_to_buffer(buffer, data.capture_scene);
    write_to_buffer(buffer, data.cubemap);
    write_to_buffer(buffer, data.irradiance);
    write_to_buffer(buffer, data.prefilter);
}

ENGINE_API refl::Type* refl::TypeResolver<SkyLight>::get() {
	static refl::Struct type = init_SkyLight();
	return &type;
}
refl::Struct init_Skybox() {
	refl::Struct type("Skybox", sizeof(Skybox));
	type.fields.append({"cubemap", offsetof(Skybox, cubemap), refl::TypeResolver<cubemap_handle>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Skybox& data) {
    write_to_buffer(buffer, data.cubemap);
}

ENGINE_API refl::Type* refl::TypeResolver<Skybox>::get() {
	static refl::Struct type = init_Skybox();
	return &type;
}
refl::Struct init_TerrainControlPoint() {
	refl::Struct type("TerrainControlPoint", sizeof(TerrainControlPoint));
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, TerrainControlPoint& data) {
}

ENGINE_API refl::Type* refl::TypeResolver<TerrainControlPoint>::get() {
	static refl::Struct type = init_TerrainControlPoint();
	return &type;
}
refl::Struct init_Terrain() {
	refl::Struct type("Terrain", sizeof(Terrain));
	type.fields.append({"width", offsetof(Terrain, width), refl::TypeResolver<uint>::get() });
	type.fields.append({"height", offsetof(Terrain, height), refl::TypeResolver<uint>::get() });
	type.fields.append({"size_of_block", offsetof(Terrain, size_of_block), refl::TypeResolver<uint>::get() });
	type.fields.append({"show_control_points", offsetof(Terrain, show_control_points), refl::TypeResolver<bool>::get() });
	type.fields.append({"heightmap", offsetof(Terrain, heightmap), refl::TypeResolver<texture_handle>::get() });
	type.fields.append({"heightmap_points", offsetof(Terrain, heightmap_points), refl::TypeResolver<vector<float>>::get() });
	type.fields.append({"max_height", offsetof(Terrain, max_height), refl::TypeResolver<float>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Terrain& data) {
    write_to_buffer(buffer, data.width);
    write_to_buffer(buffer, data.height);
    write_to_buffer(buffer, data.size_of_block);
    write_to_buffer(buffer, data.show_control_points);
    write_to_buffer(buffer, data.heightmap);
	for (uint i = 0; i < data.heightmap_points.length; i++) {
         write_to_buffer(buffer, data.heightmap_points[i]);
    }
    write_to_buffer(buffer, data.max_height);
}

ENGINE_API refl::Type* refl::TypeResolver<Terrain>::get() {
	static refl::Struct type = init_Terrain();
	return &type;
}
refl::Struct init_Transform() {
	refl::Struct type("Transform", sizeof(Transform));
	type.fields.append({"position", offsetof(Transform, position), refl::TypeResolver<glm::vec3>::get() });
	type.fields.append({"rotation", offsetof(Transform, rotation), refl::TypeResolver<glm::quat>::get() });
	type.fields.append({"scale", offsetof(Transform, scale), refl::TypeResolver<glm::vec3>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Transform& data) {
    write_to_buffer(buffer, data.position);
    write_to_buffer(buffer, data.rotation);
    write_to_buffer(buffer, data.scale);
}

ENGINE_API refl::Type* refl::TypeResolver<Transform>::get() {
	static refl::Struct type = init_Transform();
	return &type;
}
refl::Struct init_StaticTransform() {
	refl::Struct type("StaticTransform", sizeof(StaticTransform));
	type.fields.append({"model_matrix", offsetof(StaticTransform, model_matrix), refl::TypeResolver<glm::mat4>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, StaticTransform& data) {
    write_to_buffer(buffer, data.model_matrix);
}

ENGINE_API refl::Type* refl::TypeResolver<StaticTransform>::get() {
	static refl::Struct type = init_StaticTransform();
	return &type;
}
refl::Struct init_LocalTransform() {
	refl::Struct type("LocalTransform", sizeof(LocalTransform));
	type.fields.append({"position", offsetof(LocalTransform, position), refl::TypeResolver<glm::vec3>::get() });
	type.fields.append({"rotation", offsetof(LocalTransform, rotation), refl::TypeResolver<glm::quat>::get() });
	type.fields.append({"scale", offsetof(LocalTransform, scale), refl::TypeResolver<glm::vec3>::get() });
	type.fields.append({"owner", offsetof(LocalTransform, owner), refl::TypeResolver<ID>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, LocalTransform& data) {
    write_to_buffer(buffer, data.position);
    write_to_buffer(buffer, data.rotation);
    write_to_buffer(buffer, data.scale);
    write_to_buffer(buffer, data.owner);
}

ENGINE_API refl::Type* refl::TypeResolver<LocalTransform>::get() {
	static refl::Struct type = init_LocalTransform();
	return &type;
}
refl::Struct init_Entity() {
	refl::Struct type("Entity", sizeof(Entity));
	type.fields.append({"enabled", offsetof(Entity, enabled), refl::TypeResolver<bool>::get() });
	type.fields.append({"layermask", offsetof(Entity, layermask), refl::TypeResolver<Layermask>::get() });
	type.fields.append({"flags", offsetof(Entity, flags), refl::TypeResolver<Flags>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Entity& data) {
    write_to_buffer(buffer, data.enabled);
    write_to_buffer(buffer, data.layermask);
    write_to_buffer(buffer, data.flags);
}

ENGINE_API refl::Type* refl::TypeResolver<Entity>::get() {
	static refl::Struct type = init_Entity();
	return &type;
}
refl::Struct init_CapsuleCollider() {
	refl::Struct type("CapsuleCollider", sizeof(CapsuleCollider));
	type.fields.append({"radius", offsetof(CapsuleCollider, radius), refl::TypeResolver<float>::get() });
	type.fields.append({"height", offsetof(CapsuleCollider, height), refl::TypeResolver<float>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, CapsuleCollider& data) {
    write_to_buffer(buffer, data.radius);
    write_to_buffer(buffer, data.height);
}

ENGINE_API refl::Type* refl::TypeResolver<CapsuleCollider>::get() {
	static refl::Struct type = init_CapsuleCollider();
	return &type;
}
refl::Struct init_SphereCollider() {
	refl::Struct type("SphereCollider", sizeof(SphereCollider));
	type.fields.append({"radius", offsetof(SphereCollider, radius), refl::TypeResolver<float>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, SphereCollider& data) {
    write_to_buffer(buffer, data.radius);
}

ENGINE_API refl::Type* refl::TypeResolver<SphereCollider>::get() {
	static refl::Struct type = init_SphereCollider();
	return &type;
}
refl::Struct init_BoxCollider() {
	refl::Struct type("BoxCollider", sizeof(BoxCollider));
	type.fields.append({"scale", offsetof(BoxCollider, scale), refl::TypeResolver<glm::vec3>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, BoxCollider& data) {
    write_to_buffer(buffer, data.scale);
}

ENGINE_API refl::Type* refl::TypeResolver<BoxCollider>::get() {
	static refl::Struct type = init_BoxCollider();
	return &type;
}
refl::Struct init_PlaneCollider() {
	refl::Struct type("PlaneCollider", sizeof(PlaneCollider));
	type.fields.append({"normal", offsetof(PlaneCollider, normal), refl::TypeResolver<glm::vec3>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, PlaneCollider& data) {
    write_to_buffer(buffer, data.normal);
}

ENGINE_API refl::Type* refl::TypeResolver<PlaneCollider>::get() {
	static refl::Struct type = init_PlaneCollider();
	return &type;
}
refl::Struct init_RigidBody() {
	refl::Struct type("RigidBody", sizeof(RigidBody));
	type.fields.append({"mass", offsetof(RigidBody, mass), refl::TypeResolver<float>::get() });
	type.fields.append({"velocity", offsetof(RigidBody, velocity), refl::TypeResolver<glm::vec3>::get() });
	type.fields.append({"override_position", offsetof(RigidBody, override_position), refl::TypeResolver<bool>::get() });
	type.fields.append({"override_rotation", offsetof(RigidBody, override_rotation), refl::TypeResolver<bool>::get() });
	type.fields.append({"override_velocity_x", offsetof(RigidBody, override_velocity_x), refl::TypeResolver<bool>::get() });
	type.fields.append({"override_velocity_y", offsetof(RigidBody, override_velocity_y), refl::TypeResolver<bool>::get() });
	type.fields.append({"override_velocity_z", offsetof(RigidBody, override_velocity_z), refl::TypeResolver<bool>::get() });
	type.fields.append({"continous", offsetof(RigidBody, continous), refl::TypeResolver<bool>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, RigidBody& data) {
    write_to_buffer(buffer, data.mass);
    write_to_buffer(buffer, data.velocity);
    write_to_buffer(buffer, data.override_position);
    write_to_buffer(buffer, data.override_rotation);
    write_to_buffer(buffer, data.override_velocity_x);
    write_to_buffer(buffer, data.override_velocity_y);
    write_to_buffer(buffer, data.override_velocity_z);
    write_to_buffer(buffer, data.continous);
}

ENGINE_API refl::Type* refl::TypeResolver<RigidBody>::get() {
	static refl::Struct type = init_RigidBody();
	return &type;
}
refl::Struct init_CharacterController() {
	refl::Struct type("CharacterController", sizeof(CharacterController));
	type.fields.append({"on_ground", offsetof(CharacterController, on_ground), refl::TypeResolver<bool>::get() });
	type.fields.append({"velocity", offsetof(CharacterController, velocity), refl::TypeResolver<glm::vec3>::get() });
	type.fields.append({"feet_height", offsetof(CharacterController, feet_height), refl::TypeResolver<float>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, CharacterController& data) {
    write_to_buffer(buffer, data.on_ground);
    write_to_buffer(buffer, data.velocity);
    write_to_buffer(buffer, data.feet_height);
}

ENGINE_API refl::Type* refl::TypeResolver<CharacterController>::get() {
	static refl::Struct type = init_CharacterController();
	return &type;
}
refl::Struct init_MaterialDesc() {
	refl::Struct type("MaterialDesc", sizeof(MaterialDesc));
	type.fields.append({"shader", offsetof(MaterialDesc, shader), refl::TypeResolver<shader_handle>::get() });
	type.fields.append({"mode", offsetof(MaterialDesc, mode), refl::TypeResolver<MaterialDesc::Mode>::get() });
	type.fields.append({"draw_state", offsetof(MaterialDesc, draw_state), refl::TypeResolver<DrawCommandState>::get() });
	type.fields.append({"params", offsetof(MaterialDesc, params), refl::TypeResolver<array<10,ParamDesc>>::get() });
	type.fields.append({"flags", offsetof(MaterialDesc, flags), refl::TypeResolver<uint>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, MaterialDesc& data) {
    write_to_buffer(buffer, data.shader);
    write_to_buffer(buffer, data.mode);
    write_to_buffer(buffer, data.draw_state);
	for (uint i = 0; i < data.params.length; i++) {
         write_to_buffer(buffer, data.params[i]);
    }
    write_to_buffer(buffer, data.flags);
}

ENGINE_API refl::Type* refl::TypeResolver<MaterialDesc>::get() {
	static refl::Struct type = init_MaterialDesc();
	return &type;
}
refl::Struct init_Materials() {
	refl::Struct type("Materials", sizeof(Materials));
	type.fields.append({"materials", offsetof(Materials, materials), refl::TypeResolver<vector<material_handle>>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Materials& data) {
	for (uint i = 0; i < data.materials.length; i++) {
         write_to_buffer(buffer, data.materials[i]);
    }
}

ENGINE_API refl::Type* refl::TypeResolver<Materials>::get() {
	static refl::Struct type = init_Materials();
	return &type;
}
refl::Struct init_ModelRenderer() {
	refl::Struct type("ModelRenderer", sizeof(ModelRenderer));
	type.fields.append({"visible", offsetof(ModelRenderer, visible), refl::TypeResolver<bool>::get() });
	type.fields.append({"model_id", offsetof(ModelRenderer, model_id), refl::TypeResolver<model_handle>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, ModelRenderer& data) {
    write_to_buffer(buffer, data.visible);
    write_to_buffer(buffer, data.model_id);
}

ENGINE_API refl::Type* refl::TypeResolver<ModelRenderer>::get() {
	static refl::Struct type = init_ModelRenderer();
	return &type;
}
refl::Struct init_ShaderInfo() {
	refl::Struct type("ShaderInfo", sizeof(ShaderInfo));
	type.fields.append({"vfilename", offsetof(ShaderInfo, vfilename), refl::TypeResolver<sstring>::get() });
	type.fields.append({"ffilename", offsetof(ShaderInfo, ffilename), refl::TypeResolver<sstring>::get() });
	type.fields.append({"v_time_modified", offsetof(ShaderInfo, v_time_modified), refl::TypeResolver<u64>::get() });
	type.fields.append({"f_time_modified", offsetof(ShaderInfo, f_time_modified), refl::TypeResolver<u64>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, ShaderInfo& data) {
    write_to_buffer(buffer, data.vfilename);
    write_to_buffer(buffer, data.ffilename);
}

ENGINE_API refl::Type* refl::TypeResolver<ShaderInfo>::get() {
	static refl::Struct type = init_ShaderInfo();
	return &type;
}
