#pragma once
#include "core/core.h"

#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"

 refl::Enum* get_AssetNode_Type_type();
 void write_EntityNode_to_buffer(struct SerializerBuffer& buffer, struct EntityNode& data); 
 void read_EntityNode_from_buffer(struct DeserializerBuffer& buffer, struct EntityNode& data); 
 refl::Struct* get_EntityNode_type();
 void write_asset_handle_to_buffer(struct SerializerBuffer& buffer, struct asset_handle& data); 
 void read_asset_handle_from_buffer(struct DeserializerBuffer& buffer, struct asset_handle& data); 
 refl::Struct* get_asset_handle_type();
 void write_RotatablePreview_to_buffer(struct SerializerBuffer& buffer, struct RotatablePreview& data); 
 void read_RotatablePreview_from_buffer(struct DeserializerBuffer& buffer, struct RotatablePreview& data); 
 refl::Struct* get_RotatablePreview_type();
 void write_ShaderAsset_to_buffer(struct SerializerBuffer& buffer, struct ShaderAsset& data); 
 void read_ShaderAsset_from_buffer(struct DeserializerBuffer& buffer, struct ShaderAsset& data); 
 refl::Struct* get_ShaderAsset_type();
 void write_MaterialAsset_to_buffer(struct SerializerBuffer& buffer, struct MaterialAsset& data); 
 void read_MaterialAsset_from_buffer(struct DeserializerBuffer& buffer, struct MaterialAsset& data); 
 refl::Struct* get_MaterialAsset_type();
 void write_TextureAsset_to_buffer(struct SerializerBuffer& buffer, struct TextureAsset& data); 
 void read_TextureAsset_from_buffer(struct DeserializerBuffer& buffer, struct TextureAsset& data); 
 refl::Struct* get_TextureAsset_type();
 void write_ModelAsset_to_buffer(struct SerializerBuffer& buffer, struct ModelAsset& data); 
 void read_ModelAsset_from_buffer(struct DeserializerBuffer& buffer, struct ModelAsset& data); 
 refl::Struct* get_ModelAsset_type();
 void write_AssetFolder_to_buffer(struct SerializerBuffer& buffer, struct AssetFolder& data); 
 void read_AssetFolder_from_buffer(struct DeserializerBuffer& buffer, struct AssetFolder& data); 
 refl::Struct* get_AssetFolder_type();
 void write_AssetNode_to_buffer(struct SerializerBuffer& buffer, struct AssetNode& data); 
 void read_AssetNode_from_buffer(struct DeserializerBuffer& buffer, struct AssetNode& data); 
 refl::Struct* get_AssetNode_type();
