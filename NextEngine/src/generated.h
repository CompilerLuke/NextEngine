#pragma once
#include "core/core.h"

#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"
#include "components/camera.h"
#include "components/flyover.h"
#include "components/grass.h"
#include "components/lights.h"
#include "components/skybox.h"
#include "components/terrain.h"
#include "components/transform.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/assets_store.h"
#include "graphics/assets/material.h"
#include "graphics/assets/model.h"
#include "graphics/assets/shader.h"
#include "graphics/assets/texture.h"
#include "ecs/ecs20.h"
#include "core/handle.h"
#include "physics/physics.h"

template<>
struct refl::TypeResolver<MaterialDesc::Mode> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Camera& data); 
template<>
struct refl::TypeResolver<Camera> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Flyover& data); 
template<>
struct refl::TypeResolver<Flyover> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Grass& data); 
template<>
struct refl::TypeResolver<Grass> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct DirLight& data); 
template<>
struct refl::TypeResolver<DirLight> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct PointLight& data); 
template<>
struct refl::TypeResolver<PointLight> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct SkyLight& data); 
template<>
struct refl::TypeResolver<SkyLight> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Skybox& data); 
template<>
struct refl::TypeResolver<Skybox> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct TerrainControlPoint& data); 
template<>
struct refl::TypeResolver<TerrainControlPoint> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Terrain& data); 
template<>
struct refl::TypeResolver<Terrain> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Transform& data); 
template<>
struct refl::TypeResolver<Transform> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct StaticTransform& data); 
template<>
struct refl::TypeResolver<StaticTransform> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct LocalTransform& data); 
template<>
struct refl::TypeResolver<LocalTransform> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct MaterialDesc& data); 
template<>
struct refl::TypeResolver<MaterialDesc> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Materials& data); 
template<>
struct refl::TypeResolver<Materials> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct ModelRenderer& data); 
template<>
struct refl::TypeResolver<ModelRenderer> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct ShaderInfo& data); 
template<>
struct refl::TypeResolver<ShaderInfo> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Entity& data); 
template<>
struct refl::TypeResolver<Entity> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct model_handle& data); 
template<>
struct refl::TypeResolver<model_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct texture_handle& data); 
template<>
struct refl::TypeResolver<texture_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct shader_handle& data); 
template<>
struct refl::TypeResolver<shader_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct cubemap_handle& data); 
template<>
struct refl::TypeResolver<cubemap_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct material_handle& data); 
template<>
struct refl::TypeResolver<material_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct env_probe_handle& data); 
template<>
struct refl::TypeResolver<env_probe_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct shader_config_handle& data); 
template<>
struct refl::TypeResolver<shader_config_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct uniform_handle& data); 
template<>
struct refl::TypeResolver<uniform_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct pipeline_handle& data); 
template<>
struct refl::TypeResolver<pipeline_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct pipeline_layout_handle& data); 
template<>
struct refl::TypeResolver<pipeline_layout_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct descriptor_set_handle& data); 
template<>
struct refl::TypeResolver<descriptor_set_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct render_pass_handle& data); 
template<>
struct refl::TypeResolver<render_pass_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct command_buffer_handle& data); 
template<>
struct refl::TypeResolver<command_buffer_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct frame_buffer_handle& data); 
template<>
struct refl::TypeResolver<frame_buffer_handle> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct CapsuleCollider& data); 
template<>
struct refl::TypeResolver<CapsuleCollider> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct SphereCollider& data); 
template<>
struct refl::TypeResolver<SphereCollider> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct BoxCollider& data); 
template<>
struct refl::TypeResolver<BoxCollider> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct PlaneCollider& data); 
template<>
struct refl::TypeResolver<PlaneCollider> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct RigidBody& data); 
template<>
struct refl::TypeResolver<RigidBody> {
	ENGINE_API static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct CharacterController& data); 
template<>
struct refl::TypeResolver<CharacterController> {
	ENGINE_API static refl::Type* get();
};

