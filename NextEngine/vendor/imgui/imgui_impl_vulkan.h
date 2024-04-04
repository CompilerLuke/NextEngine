#pragma once

// Specific OpenGL versions
//#define IMGUI_IMPL_OPENGL_ES2     // Auto-detected on Emscripten
//#define IMGUI_IMPL_OPENGL_ES3     // Auto-detected on iOS/Android

#include "imconfig.h"

IMGUI_IMPL_API bool     ImGui_ImplVulkan_Init(const char* glsl_version = NULL);
IMGUI_IMPL_API void     ImGui_ImplVulkan_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplVulkan_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplVulkan_RenderDrawData(struct CommandBuffer& cmd_buffer, ImDrawData* draw_data);

// Called by Init/NewFrame/Shutdown
IMGUI_IMPL_API bool     ImGui_ImplVulkan_CreateFontsTexture();
IMGUI_IMPL_API void     ImGui_ImplVulkan_DestroyFontsTexture();
IMGUI_IMPL_API bool     ImGui_ImplVulkan_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplVulkan_DestroyDeviceObjects();