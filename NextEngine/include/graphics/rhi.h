#pragma once

#include "shaderManager.h"
#include "textureManager.h"
#include "model/modelManager.h"
#include "materialManager.h"

namespace RHI {
	extern ShaderManager shader_manager;
	extern TextureManager texture_manager;
	extern CubemapManager cubemap_manager;
	extern ModelManager model_manager;
	extern MaterialManager material_manager;
};