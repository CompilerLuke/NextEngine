#pragma once

#include "volk.h"

struct Window;
struct Level;
struct ModelManager;
struct RHI;

struct VulkanDesc {
	const char* app_name = "No name";
	const char* engine_name = "No name";

	uint32_t engine_version;
	uint32_t app_version;
	uint32_t api_version;

	uint32_t min_log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	uint32_t num_validation_layers = 0;
	const char** validation_layers;

	VkPhysicalDeviceFeatures device_features;
};

struct FrameData {
	glm::mat4 model_matrix;
	glm::mat4 view_matrix;
	glm::mat4 proj_matrix;
};

//todo abstract VulkanDesc
RHI* make_RHI(const VulkanDesc&, ModelManager& model_manager, Level&, Window&);
void vk_draw_frame(RHI& rhi, FrameData& data);
void destroy_RHI(RHI* rhi);

#define VK_CHECK(x)  \
{ \
	VkResult result = x; \
	if (result != VK_SUCCESS) { \
		printf("Vulkan Error %d at %s:%d", result, __FILE__, __LINE__); \
		abort(); \
	} \
}