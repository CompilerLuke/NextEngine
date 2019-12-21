#pragma once

#include "core/core.h"
#include "core/HandleManager.h"
#include "model/modelManager.h"
#include "shaderManager.h"
#include "textureManager.h"
#include "materialManager.h"

namespace RHI {
	extern ShaderManager ENGINE_API shader_manager;
	extern TextureManager ENGINE_API texture_manager;
	extern CubemapManager ENGINE_API cubemap_manager;
	extern ModelManager ENGINE_API model_manager;
	extern MaterialManager ENGINE_API material_manager;
};

