#include "stdafx.h"
#include "graphics/rhi.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "model/model.h"
#include "graphics/materialSystem.h"

ShaderManager RHI::shader_manager;
TextureManager RHI::texture_manager;
CubemapManager RHI::cubemap_manager;
ModelManager RHI::model_manager;
MaterialManager RHI::material_manager;