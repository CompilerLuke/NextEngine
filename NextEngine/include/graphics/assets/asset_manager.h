#pragma once

#include "material_manager.h"
#include "model.h"
#include "texture.h"
#include "shader.h"

struct AssetManager {
	ModelManager models;
	TextureManager textures;
	CubemapManager cubemaps;
	MaterialManager materials;
	ShaderManager shaders;
};