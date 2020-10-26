#pragma once
#include "core/core.h"

#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"

ENGINE_API refl::Enum* get_MaterialDesc_Mode_type();
ENGINE_API refl::Alias* get_DrawCommandState_type();
ENGINE_API void write_DrawCommandState_to_buffer(SerializerBuffer& buffer, u64& data);
ENGINE_API void read_DrawCommandState_from_buffer(DeserializerBuffer& buffer, u64& data);
ENGINE_API refl::Alias* get_shader_flags_type();
ENGINE_API void write_shader_flags_to_buffer(SerializerBuffer& buffer, u64& data);
ENGINE_API void read_shader_flags_from_buffer(DeserializerBuffer& buffer, u64& data);
ENGINE_API refl::Alias* get_ID_type();
ENGINE_API void write_ID_to_buffer(SerializerBuffer& buffer, uint& data);
ENGINE_API void read_ID_from_buffer(DeserializerBuffer& buffer, uint& data);
ENGINE_API refl::Alias* get_Layermask_type();
ENGINE_API void write_Layermask_to_buffer(SerializerBuffer& buffer, u64& data);
ENGINE_API void read_Layermask_from_buffer(DeserializerBuffer& buffer, u64& data);
ENGINE_API refl::Alias* get_Archetype_type();
ENGINE_API void write_Archetype_to_buffer(SerializerBuffer& buffer, u64& data);
ENGINE_API void read_Archetype_from_buffer(DeserializerBuffer& buffer, u64& data);
ENGINE_API refl::Alias* get_EntityFlags_type();
ENGINE_API void write_EntityFlags_to_buffer(SerializerBuffer& buffer, u64& data);
ENGINE_API void read_EntityFlags_from_buffer(DeserializerBuffer& buffer, u64& data);
ENGINE_API refl::Enum* get_Param_Type_type();
ENGINE_API void write_Transform_to_buffer(struct SerializerBuffer& buffer, struct Transform& data); 
ENGINE_API void read_Transform_from_buffer(struct DeserializerBuffer& buffer, struct Transform& data); 
ENGINE_API refl::Struct* get_Transform_type();
ENGINE_API void write_StaticTransform_to_buffer(struct SerializerBuffer& buffer, struct StaticTransform& data); 
ENGINE_API void read_StaticTransform_from_buffer(struct DeserializerBuffer& buffer, struct StaticTransform& data); 
ENGINE_API refl::Struct* get_StaticTransform_type();
ENGINE_API void write_LocalTransform_to_buffer(struct SerializerBuffer& buffer, struct LocalTransform& data); 
ENGINE_API void read_LocalTransform_from_buffer(struct DeserializerBuffer& buffer, struct LocalTransform& data); 
ENGINE_API refl::Struct* get_LocalTransform_type();
ENGINE_API void write_Camera_to_buffer(struct SerializerBuffer& buffer, struct Camera& data); 
ENGINE_API void read_Camera_from_buffer(struct DeserializerBuffer& buffer, struct Camera& data); 
ENGINE_API refl::Struct* get_Camera_type();
ENGINE_API void write_Flyover_to_buffer(struct SerializerBuffer& buffer, struct Flyover& data); 
ENGINE_API void read_Flyover_from_buffer(struct DeserializerBuffer& buffer, struct Flyover& data); 
ENGINE_API refl::Struct* get_Flyover_type();
ENGINE_API void write_Skybox_to_buffer(struct SerializerBuffer& buffer, struct Skybox& data); 
ENGINE_API void read_Skybox_from_buffer(struct DeserializerBuffer& buffer, struct Skybox& data); 
ENGINE_API refl::Struct* get_Skybox_type();
ENGINE_API void write_TerrainControlPoint_to_buffer(struct SerializerBuffer& buffer, struct TerrainControlPoint& data); 
ENGINE_API void read_TerrainControlPoint_from_buffer(struct DeserializerBuffer& buffer, struct TerrainControlPoint& data); 
ENGINE_API refl::Struct* get_TerrainControlPoint_type();
ENGINE_API void write_TerrainSplat_to_buffer(struct SerializerBuffer& buffer, struct TerrainSplat& data); 
ENGINE_API void read_TerrainSplat_from_buffer(struct DeserializerBuffer& buffer, struct TerrainSplat& data); 
ENGINE_API refl::Struct* get_TerrainSplat_type();
ENGINE_API void write_TerrainMaterial_to_buffer(struct SerializerBuffer& buffer, struct TerrainMaterial& data); 
ENGINE_API void read_TerrainMaterial_from_buffer(struct DeserializerBuffer& buffer, struct TerrainMaterial& data); 
ENGINE_API refl::Struct* get_TerrainMaterial_type();
ENGINE_API void write_Terrain_to_buffer(struct SerializerBuffer& buffer, struct Terrain& data); 
ENGINE_API void read_Terrain_from_buffer(struct DeserializerBuffer& buffer, struct Terrain& data); 
ENGINE_API refl::Struct* get_Terrain_type();
ENGINE_API void write_DirLight_to_buffer(struct SerializerBuffer& buffer, struct DirLight& data); 
ENGINE_API void read_DirLight_from_buffer(struct DeserializerBuffer& buffer, struct DirLight& data); 
ENGINE_API refl::Struct* get_DirLight_type();
ENGINE_API void write_PointLight_to_buffer(struct SerializerBuffer& buffer, struct PointLight& data); 
ENGINE_API void read_PointLight_from_buffer(struct DeserializerBuffer& buffer, struct PointLight& data); 
ENGINE_API refl::Struct* get_PointLight_type();
ENGINE_API void write_SkyLight_to_buffer(struct SerializerBuffer& buffer, struct SkyLight& data); 
ENGINE_API void read_SkyLight_from_buffer(struct DeserializerBuffer& buffer, struct SkyLight& data); 
ENGINE_API refl::Struct* get_SkyLight_type();
ENGINE_API void write_Grass_to_buffer(struct SerializerBuffer& buffer, struct Grass& data); 
ENGINE_API void read_Grass_from_buffer(struct DeserializerBuffer& buffer, struct Grass& data); 
ENGINE_API refl::Struct* get_Grass_type();
ENGINE_API void write_model_handle_to_buffer(struct SerializerBuffer& buffer, struct model_handle& data); 
ENGINE_API void read_model_handle_from_buffer(struct DeserializerBuffer& buffer, struct model_handle& data); 
ENGINE_API refl::Struct* get_model_handle_type();
ENGINE_API void write_texture_handle_to_buffer(struct SerializerBuffer& buffer, struct texture_handle& data); 
ENGINE_API void read_texture_handle_from_buffer(struct DeserializerBuffer& buffer, struct texture_handle& data); 
ENGINE_API refl::Struct* get_texture_handle_type();
ENGINE_API void write_shader_handle_to_buffer(struct SerializerBuffer& buffer, struct shader_handle& data); 
ENGINE_API void read_shader_handle_from_buffer(struct DeserializerBuffer& buffer, struct shader_handle& data); 
ENGINE_API refl::Struct* get_shader_handle_type();
ENGINE_API void write_cubemap_handle_to_buffer(struct SerializerBuffer& buffer, struct cubemap_handle& data); 
ENGINE_API void read_cubemap_handle_from_buffer(struct DeserializerBuffer& buffer, struct cubemap_handle& data); 
ENGINE_API refl::Struct* get_cubemap_handle_type();
ENGINE_API void write_material_handle_to_buffer(struct SerializerBuffer& buffer, struct material_handle& data); 
ENGINE_API void read_material_handle_from_buffer(struct DeserializerBuffer& buffer, struct material_handle& data); 
ENGINE_API refl::Struct* get_material_handle_type();
ENGINE_API void write_env_probe_handle_to_buffer(struct SerializerBuffer& buffer, struct env_probe_handle& data); 
ENGINE_API void read_env_probe_handle_from_buffer(struct DeserializerBuffer& buffer, struct env_probe_handle& data); 
ENGINE_API refl::Struct* get_env_probe_handle_type();
ENGINE_API void write_shader_config_handle_to_buffer(struct SerializerBuffer& buffer, struct shader_config_handle& data); 
ENGINE_API void read_shader_config_handle_from_buffer(struct DeserializerBuffer& buffer, struct shader_config_handle& data); 
ENGINE_API refl::Struct* get_shader_config_handle_type();
ENGINE_API void write_uniform_handle_to_buffer(struct SerializerBuffer& buffer, struct uniform_handle& data); 
ENGINE_API void read_uniform_handle_from_buffer(struct DeserializerBuffer& buffer, struct uniform_handle& data); 
ENGINE_API refl::Struct* get_uniform_handle_type();
ENGINE_API void write_pipeline_handle_to_buffer(struct SerializerBuffer& buffer, struct pipeline_handle& data); 
ENGINE_API void read_pipeline_handle_from_buffer(struct DeserializerBuffer& buffer, struct pipeline_handle& data); 
ENGINE_API refl::Struct* get_pipeline_handle_type();
ENGINE_API void write_pipeline_layout_handle_to_buffer(struct SerializerBuffer& buffer, struct pipeline_layout_handle& data); 
ENGINE_API void read_pipeline_layout_handle_from_buffer(struct DeserializerBuffer& buffer, struct pipeline_layout_handle& data); 
ENGINE_API refl::Struct* get_pipeline_layout_handle_type();
ENGINE_API void write_descriptor_set_handle_to_buffer(struct SerializerBuffer& buffer, struct descriptor_set_handle& data); 
ENGINE_API void read_descriptor_set_handle_from_buffer(struct DeserializerBuffer& buffer, struct descriptor_set_handle& data); 
ENGINE_API refl::Struct* get_descriptor_set_handle_type();
ENGINE_API void write_render_pass_handle_to_buffer(struct SerializerBuffer& buffer, struct render_pass_handle& data); 
ENGINE_API void read_render_pass_handle_from_buffer(struct DeserializerBuffer& buffer, struct render_pass_handle& data); 
ENGINE_API refl::Struct* get_render_pass_handle_type();
ENGINE_API void write_command_buffer_handle_to_buffer(struct SerializerBuffer& buffer, struct command_buffer_handle& data); 
ENGINE_API void read_command_buffer_handle_from_buffer(struct DeserializerBuffer& buffer, struct command_buffer_handle& data); 
ENGINE_API refl::Struct* get_command_buffer_handle_type();
ENGINE_API void write_frame_buffer_handle_to_buffer(struct SerializerBuffer& buffer, struct frame_buffer_handle& data); 
ENGINE_API void read_frame_buffer_handle_from_buffer(struct DeserializerBuffer& buffer, struct frame_buffer_handle& data); 
ENGINE_API refl::Struct* get_frame_buffer_handle_type();
ENGINE_API void write_CapsuleCollider_to_buffer(struct SerializerBuffer& buffer, struct CapsuleCollider& data); 
ENGINE_API void read_CapsuleCollider_from_buffer(struct DeserializerBuffer& buffer, struct CapsuleCollider& data); 
ENGINE_API refl::Struct* get_CapsuleCollider_type();
ENGINE_API void write_SphereCollider_to_buffer(struct SerializerBuffer& buffer, struct SphereCollider& data); 
ENGINE_API void read_SphereCollider_from_buffer(struct DeserializerBuffer& buffer, struct SphereCollider& data); 
ENGINE_API refl::Struct* get_SphereCollider_type();
ENGINE_API void write_BoxCollider_to_buffer(struct SerializerBuffer& buffer, struct BoxCollider& data); 
ENGINE_API void read_BoxCollider_from_buffer(struct DeserializerBuffer& buffer, struct BoxCollider& data); 
ENGINE_API refl::Struct* get_BoxCollider_type();
ENGINE_API void write_PlaneCollider_to_buffer(struct SerializerBuffer& buffer, struct PlaneCollider& data); 
ENGINE_API void read_PlaneCollider_from_buffer(struct DeserializerBuffer& buffer, struct PlaneCollider& data); 
ENGINE_API refl::Struct* get_PlaneCollider_type();
ENGINE_API void write_RigidBody_to_buffer(struct SerializerBuffer& buffer, struct RigidBody& data); 
ENGINE_API void read_RigidBody_from_buffer(struct DeserializerBuffer& buffer, struct RigidBody& data); 
ENGINE_API refl::Struct* get_RigidBody_type();
ENGINE_API void write_CharacterController_to_buffer(struct SerializerBuffer& buffer, struct CharacterController& data); 
ENGINE_API void read_CharacterController_from_buffer(struct DeserializerBuffer& buffer, struct CharacterController& data); 
ENGINE_API refl::Struct* get_CharacterController_type();
ENGINE_API void write_ParamDesc_to_buffer(struct SerializerBuffer& buffer, struct ParamDesc& data); 
ENGINE_API void read_ParamDesc_from_buffer(struct DeserializerBuffer& buffer, struct ParamDesc& data); 
ENGINE_API refl::Struct* get_ParamDesc_type();
ENGINE_API void write_MaterialDesc_to_buffer(struct SerializerBuffer& buffer, struct MaterialDesc& data); 
ENGINE_API void read_MaterialDesc_from_buffer(struct DeserializerBuffer& buffer, struct MaterialDesc& data); 
ENGINE_API refl::Struct* get_MaterialDesc_type();
ENGINE_API void write_Materials_to_buffer(struct SerializerBuffer& buffer, struct Materials& data); 
ENGINE_API void read_Materials_from_buffer(struct DeserializerBuffer& buffer, struct Materials& data); 
ENGINE_API refl::Struct* get_Materials_type();
ENGINE_API void write_ModelRenderer_to_buffer(struct SerializerBuffer& buffer, struct ModelRenderer& data); 
ENGINE_API void read_ModelRenderer_from_buffer(struct DeserializerBuffer& buffer, struct ModelRenderer& data); 
ENGINE_API refl::Struct* get_ModelRenderer_type();
ENGINE_API void write_ShaderInfo_to_buffer(struct SerializerBuffer& buffer, struct ShaderInfo& data); 
ENGINE_API void read_ShaderInfo_from_buffer(struct DeserializerBuffer& buffer, struct ShaderInfo& data); 
ENGINE_API refl::Struct* get_ShaderInfo_type();
ENGINE_API void write_Entity_to_buffer(struct SerializerBuffer& buffer, struct Entity& data); 
ENGINE_API void read_Entity_from_buffer(struct DeserializerBuffer& buffer, struct Entity& data); 
ENGINE_API refl::Struct* get_Entity_type();
ENGINE_API void write_ArchetypeStore_to_buffer(struct SerializerBuffer& buffer, struct ArchetypeStore& data); 
ENGINE_API void read_ArchetypeStore_from_buffer(struct DeserializerBuffer& buffer, struct ArchetypeStore& data); 
ENGINE_API refl::Struct* get_ArchetypeStore_type();