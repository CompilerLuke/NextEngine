// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once



#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#ifdef RENDER_API_GL
#include <glad/glad.h>
#endif

#ifdef NE_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#endif

#include "graphics/rhi/vulkan/volk.h"

#ifdef RENDER_API_VULKAN
#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

// TODO: reference additional headers your program requires here

