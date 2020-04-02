#pragma once

#include "core/io/vfs.h"
#include "material_manager.h"
#include "model.h"
#include "texture.h"
#include "shader.h"

struct ENGINE_API AssetManager {
	AssetManager(Level& level);

	Level& level;
	ModelManager models;
	TextureManager textures;
	CubemapManager cubemaps;
	MaterialManager materials;
	ShaderManager shaders;
};