#include "generated.h"
#include <core/types.h>
#include "lister.h"
#include "assetTab.h"

refl::Enum init_AssetNode_Type() {
	refl::Enum type("Type", sizeof(AssetNode::Type));
	type.values.append({ "Texture", AssetNode::Type::Texture });
	type.values.append({ "Material", AssetNode::Type::Material });
	type.values.append({ "Shader", AssetNode::Type::Shader });
	type.values.append({ "Model", AssetNode::Type::Model });
	type.values.append({ "Folder", AssetNode::Type::Folder });
	return type;
}

refl::Enum* get_AssetNode_Type_type() {
	static refl::Enum type = init_AssetNode_Type();
	return &type;
}
refl::Struct init_EntityNode() {
	refl::Struct type("EntityNode", sizeof(EntityNode));
	type.fields.append({"name", offsetof(EntityNode, name), get_sstring_type()});
	type.fields.append({"id", offsetof(EntityNode, id), get_ID_type()});
	type.fields.append({"expanded", offsetof(EntityNode, expanded), get_bool_type()});
	type.fields.append({"children", offsetof(EntityNode, children), make_vector_type(get_EntityNode_type())});
	type.fields.append({"parent", offsetof(EntityNode, parent), get_int_type()});
	return type;
}

void write_EntityNode_to_buffer(SerializerBuffer& buffer, EntityNode& data) {
    write_sstring_to_buffer(buffer, data.name);
    write_ID_to_buffer(buffer, data.id);
    write_n_to_buffer(buffer, &data.expanded, sizeof(bool));
    write_uint_to_buffer(buffer, data.children.length);
	for (uint i = 0; i < data.children.length; i++) {
         write_EntityNode_to_buffer(buffer, data.children[i]);
    }
    write_n_to_buffer(buffer, &data.parent, sizeof(int));
}

void read_EntityNode_from_buffer(DeserializerBuffer& buffer, EntityNode& data) {
    read_sstring_from_buffer(buffer, data.name);
    read_ID_from_buffer(buffer, data.id);
    read_n_from_buffer(buffer, &data.expanded, sizeof(bool));
    data.children.resize(read_uint_from_buffer(buffer));
	for (uint i = 0; i < data.children.length; i++) {
         read_EntityNode_from_buffer(buffer, data.children[i]);
    }
    read_n_from_buffer(buffer, &data.parent, sizeof(int));
}

refl::Struct* get_EntityNode_type() {
	static refl::Struct type = init_EntityNode();
	return &type;
}

refl::Struct init_TextureAsset() {
	refl::Struct type("TextureAsset", sizeof(TextureAsset));
	type.fields.append({"handle", offsetof(TextureAsset, handle), get_texture_handle_type()});
	type.fields.append({"name", offsetof(TextureAsset, name), get_sstring_type()});
	type.fields.append({"path", offsetof(TextureAsset, path), get_sstring_type()});
	return type;
}

void write_TextureAsset_to_buffer(SerializerBuffer& buffer, TextureAsset& data) {
    write_n_to_buffer(buffer, &data, sizeof(TextureAsset));
}

void read_TextureAsset_from_buffer(DeserializerBuffer& buffer, TextureAsset& data) {
    read_n_from_buffer(buffer, &data, sizeof(TextureAsset));
}

refl::Struct* get_TextureAsset_type() {
	static refl::Struct type = init_TextureAsset();
	return &type;
}

refl::Struct init_RotatablePreview() {
	refl::Struct type("RotatablePreview", sizeof(RotatablePreview));
	type.fields.append({"preview_tex_coord", offsetof(RotatablePreview, preview_tex_coord), get_vec2_type()});
	type.fields.append({"current", offsetof(RotatablePreview, current), get_vec2_type()});
	type.fields.append({"previous", offsetof(RotatablePreview, previous), get_vec2_type()});
	type.fields.append({"rot_deg", offsetof(RotatablePreview, rot_deg), get_vec2_type()});
	type.fields.append({"rot", offsetof(RotatablePreview, rot), get_quat_type()});
	return type;
}

void write_RotatablePreview_to_buffer(SerializerBuffer& buffer, RotatablePreview& data) {
    write_n_to_buffer(buffer, &data, sizeof(RotatablePreview));
}

void read_RotatablePreview_from_buffer(DeserializerBuffer& buffer, RotatablePreview& data) {
    read_n_from_buffer(buffer, &data, sizeof(RotatablePreview));
}

refl::Struct* get_RotatablePreview_type() {
	static refl::Struct type = init_RotatablePreview();
	return &type;
}

refl::Struct init_ModelAsset() {
	refl::Struct type("ModelAsset", sizeof(ModelAsset));
	type.fields.append({"handle", offsetof(ModelAsset, handle), get_model_handle_type()});
	type.fields.append({"name", offsetof(ModelAsset, name), get_sstring_type()});
	type.fields.append({"rot_preview", offsetof(ModelAsset, rot_preview), get_RotatablePreview_type()});
	type.fields.append({"path", offsetof(ModelAsset, path), get_sstring_type()});
	type.fields.append({"materials", offsetof(ModelAsset, materials), make_array_type(8, sizeof(array<8, struct material_handle>), get_material_handle_type())});
	type.fields.append({"trans", offsetof(ModelAsset, trans), get_Transform_type()});
	type.fields.append({"lod_distance", offsetof(ModelAsset, lod_distance), make_array_type(10, sizeof(array<10, float>), get_float_type())});
	return type;
}

void write_ModelAsset_to_buffer(SerializerBuffer& buffer, ModelAsset& data) {
    write_n_to_buffer(buffer, &data, sizeof(ModelAsset));
}

void read_ModelAsset_from_buffer(DeserializerBuffer& buffer, ModelAsset& data) {
    read_n_from_buffer(buffer, &data, sizeof(ModelAsset));
}

refl::Struct* get_ModelAsset_type() {
	static refl::Struct type = init_ModelAsset();
	return &type;
}

refl::Struct init_ShaderAsset() {
	refl::Struct type("ShaderAsset", sizeof(ShaderAsset));
	type.fields.append({"handle", offsetof(ShaderAsset, handle), get_shader_handle_type()});
	type.fields.append({"name", offsetof(ShaderAsset, name), get_sstring_type()});
	type.fields.append({"path", offsetof(ShaderAsset, path), get_sstring_type()});
	type.fields.append({"shader_arguments", offsetof(ShaderAsset, shader_arguments), make_vector_type(get_ParamDesc_type())});
	type.fields.append({"graph", offsetof(ShaderAsset, graph), get_int_type()});
	return type;
}

void write_ShaderAsset_to_buffer(SerializerBuffer& buffer, ShaderAsset& data) {
    write_shader_handle_to_buffer(buffer, data.handle);
    write_sstring_to_buffer(buffer, data.name);
    write_sstring_to_buffer(buffer, data.path);
    write_uint_to_buffer(buffer, data.shader_arguments.length);
	for (uint i = 0; i < data.shader_arguments.length; i++) {
         write_ParamDesc_to_buffer(buffer, data.shader_arguments[i]);
    }
    write_n_to_buffer(buffer, &data.graph, sizeof(int));
}

void read_ShaderAsset_from_buffer(DeserializerBuffer& buffer, ShaderAsset& data) {
    read_shader_handle_from_buffer(buffer, data.handle);
    read_sstring_from_buffer(buffer, data.name);
    read_sstring_from_buffer(buffer, data.path);
    data.shader_arguments.resize(read_uint_from_buffer(buffer));
	for (uint i = 0; i < data.shader_arguments.length; i++) {
         read_ParamDesc_from_buffer(buffer, data.shader_arguments[i]);
    }
    read_n_from_buffer(buffer, &data.graph, sizeof(int));
}

refl::Struct* get_ShaderAsset_type() {
	static refl::Struct type = init_ShaderAsset();
	return &type;
}

refl::Struct init_MaterialAsset() {
	refl::Struct type("MaterialAsset", sizeof(MaterialAsset));
	type.fields.append({"handle", offsetof(MaterialAsset, handle), get_material_handle_type()});
	type.fields.append({"name", offsetof(MaterialAsset, name), get_sstring_type()});
	type.fields.append({"rot_preview", offsetof(MaterialAsset, rot_preview), get_RotatablePreview_type()});
	type.fields.append({"desc", offsetof(MaterialAsset, desc), get_MaterialDesc_type()});
	return type;
}

void write_MaterialAsset_to_buffer(SerializerBuffer& buffer, MaterialAsset& data) {
    write_n_to_buffer(buffer, &data, sizeof(MaterialAsset));
}

void read_MaterialAsset_from_buffer(DeserializerBuffer& buffer, MaterialAsset& data) {
    read_n_from_buffer(buffer, &data, sizeof(MaterialAsset));
}

refl::Struct* get_MaterialAsset_type() {
	static refl::Struct type = init_MaterialAsset();
	return &type;
}

refl::Struct init_asset_handle() {
	refl::Struct type("asset_handle", sizeof(asset_handle));
	type.fields.append({"id", offsetof(asset_handle, id), get_uint_type()});
	return type;
}

void write_asset_handle_to_buffer(SerializerBuffer& buffer, asset_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(asset_handle));
}

void read_asset_handle_from_buffer(DeserializerBuffer& buffer, asset_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(asset_handle));
}

refl::Struct* get_asset_handle_type() {
	static refl::Struct type = init_asset_handle();
	return &type;
}

refl::Struct init_AssetFolder() {
	refl::Struct type("AssetFolder", sizeof(AssetFolder));
	type.fields.append({"name", offsetof(AssetFolder, name), get_sstring_type()});
	type.fields.append({"contents", offsetof(AssetFolder, contents), make_vector_type(get_AssetNode_type())});
	type.fields.append({"owner", offsetof(AssetFolder, owner), get_asset_handle_type()});
	return type;
}

void write_AssetFolder_to_buffer(SerializerBuffer& buffer, AssetFolder& data) {
    write_sstring_to_buffer(buffer, data.name);
    write_uint_to_buffer(buffer, data.contents.length);
	for (uint i = 0; i < data.contents.length; i++) {
         write_AssetNode_to_buffer(buffer, data.contents[i]);
    }
    write_asset_handle_to_buffer(buffer, data.owner);
}

void read_AssetFolder_from_buffer(DeserializerBuffer& buffer, AssetFolder& data) {
    read_sstring_from_buffer(buffer, data.name);
    data.contents.resize(read_uint_from_buffer(buffer));
	for (uint i = 0; i < data.contents.length; i++) {
         read_AssetNode_from_buffer(buffer, data.contents[i]);
    }
    read_asset_handle_from_buffer(buffer, data.owner);
}

refl::Struct* get_AssetFolder_type() {
	static refl::Struct type = init_AssetFolder();
	return &type;
}

refl::Struct init_AssetNode() {
	refl::Struct type("AssetNode", sizeof(AssetNode));
	type.fields.append({"handle", offsetof(AssetNode, handle), get_asset_handle_type()});
    static refl::Union inline_union = { refl::Type::Union, 0, "AssetNode" };
    type.fields.append({ "", offsetof(AssetNode, type), &inline_union });
	return type;
}

void write_AssetNode_to_buffer(SerializerBuffer& buffer, AssetNode& data) {
    write_n_to_buffer(buffer, &data.type, sizeof(int));
    write_asset_handle_to_buffer(buffer, data.handle);
    switch (data.type) {
    case AssetNode::Texture:
    write_TextureAsset_to_buffer(buffer, data.texture);
    break;

    case AssetNode::Material:
    write_MaterialAsset_to_buffer(buffer, data.material);
    break;

    case AssetNode::Shader:
    write_ShaderAsset_to_buffer(buffer, data.shader);
    break;

    case AssetNode::Model:
    write_ModelAsset_to_buffer(buffer, data.model);
    break;

    case AssetNode::Folder:
    write_AssetFolder_to_buffer(buffer, data.folder);
    break;

    }
}

void read_AssetNode_from_buffer(DeserializerBuffer& buffer, AssetNode& data) {
    AssetNode::Type type;
    read_n_from_buffer(buffer, &type, sizeof(int));
    new (&data) AssetNode(type);
    read_asset_handle_from_buffer(buffer, data.handle);
    switch (data.type) {
    case AssetNode::Texture:
    read_TextureAsset_from_buffer(buffer, data.texture);
    break;

    case AssetNode::Material:
    read_MaterialAsset_from_buffer(buffer, data.material);
    break;

    case AssetNode::Shader:
    read_ShaderAsset_from_buffer(buffer, data.shader);
    break;

    case AssetNode::Model:
    read_ModelAsset_from_buffer(buffer, data.model);
    break;

    case AssetNode::Folder:
    read_AssetFolder_from_buffer(buffer, data.folder);
    break;

    }
}

refl::Struct* get_AssetNode_type() {
	static refl::Struct type = init_AssetNode();
	return &type;
}

#include "C:\Users\User\source\repos\NextEngine\NextEngineEditor\/include/component_ids.h"
#include "ecs/ecs.h"
#include "engine/application.h"


APPLICATION_API void register_components(World& world) {

};