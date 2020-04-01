#pragma once

#include "core/io/vfs.h"
#include "material_manager.h"
#include "model.h"
#include "texture.h"
#include "shader.h"

struct AssetManager {
	AssetManager(string_view level_path);

	Level level;
	ModelManager models;
	TextureManager textures;
	CubemapManager cubemaps;
	MaterialManager materials;
	ShaderManager shaders;
};