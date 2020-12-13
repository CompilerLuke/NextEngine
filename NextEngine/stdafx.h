// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef NE_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN

    #include <WinSock2.h>
    #include <windows.h>

    #define GLFW_EXPOSE_NATIVE_WIN32
#endif

#ifdef NE_PLATFORM_MACOSX
    #define GLFW_EXPOSE_NATIVE_COCOA
#endif

#include "graphics/rhi/vulkan/volk.h"

#ifdef RENDER_API_VULKAN
#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#ifdef RENDER_API_GL
#include <glad/glad.h>
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <stb_image.h>

#include <tuple>




/*
#include "targetver.h"
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "core/container/vector.h"
#include "ecs/ecs.h"
#include "core/container/handle_manager.h"
#include "core/reflection.h"
#include <functional>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define WIN32_MEAN_AND_LEAN
#include <windows.h>
*/
