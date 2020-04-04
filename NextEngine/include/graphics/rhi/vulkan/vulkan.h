#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Window;

struct VulkanDesc {
	const char* app_name = "No name";
	const char* engine_name = "No name";

	uint32_t engine_version;
	uint32_t app_version;
	uint32_t api_version;

	uint32_t min_log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	uint32_t num_validation_layers = 0;
	const char** validation_layers;
};

void init(const VulkanDesc&, Window*);
void drawFrame();
void deinit();

