// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "core/container/vector.h"
#include <glad/glad.h>
#include "ecs/ecs.h"
#include "core/container/handle_manager.h"
#include "core/reflection.h"
#include <functional>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>