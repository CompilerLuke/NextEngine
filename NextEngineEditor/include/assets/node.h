#pragma once

#include <core/core.h>
#include <core/container/sstring.h>
#include <core/container/vector.h>
#include <engine/handle.h>
#include <components/transform.h>
#include <graphics/assets/material.h>
#include <glm/vec2.hpp>
#include <glm/gtc/quaternion.hpp>
#include "handle.h"

struct ShaderGraph;
struct AssetNode;

struct NamedAsset {
	uint handle;
	sstring name;
};

REFL
struct RotatablePreview {
	glm::vec2 preview_tex_coord;
	glm::vec2 current;
	glm::vec2 previous;
	glm::vec2 rot_deg;
	glm::quat rot;
};


REFL
struct ShaderAsset {
	shader_handle handle = { INVALID_HANDLE };
	sstring name;
	sstring path;

	vector<ParamDesc> shader_arguments;

	ShaderGraph* graph = NULL; //todo this leaks now
};

REFL
struct MaterialAsset {
	material_handle handle = { INVALID_HANDLE };
	sstring name;
	RotatablePreview rot_preview;
	MaterialDesc desc;
};

REFL
struct TextureAsset {
	texture_handle handle;
	sstring name;
	sstring path;
};

REFL
struct ModelAsset {
	model_handle handle = { INVALID_HANDLE };
	sstring name;
	RotatablePreview rot_preview;
	sstring path;

	array<8, material_handle> materials;

	Transform trans;

	array<10, float> lod_distance;
};

REFL
struct AssetFolder {
	sstring name;
	vector<AssetNode> contents;
	asset_handle owner;
};

REFL
struct AssetNode {
	enum Type { Texture, Material, Shader, Model, Folder, Count } type;
	asset_handle handle;

	union {
		TextureAsset texture;
		MaterialAsset material;
		ShaderAsset shader;
		ModelAsset model;
		AssetFolder folder;
		NamedAsset asset;
	};

	REFL_FALSE AssetNode();
	REFL_FALSE ~AssetNode();
	REFL_FALSE AssetNode(AssetNode::Type type);
	REFL_FALSE void operator=(AssetNode&&);
};

const char* drop_types[AssetNode::Count] = {
	"DRAG_AND_DROP_IMAGE",
	"DRAG_AND_DROP_MATERIAL",
	"DRAG_AND_DROP_SHADER",
	"DRAG_AND_DROP_MODEL",
	"DRAG_AND_DROP_FOLDER",
};
