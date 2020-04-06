#pragma once

#include "vulkan.h"


VkShaderModule make_ShaderModule(VkDevice device, string_view code);