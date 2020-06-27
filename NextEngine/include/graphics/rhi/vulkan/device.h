#pragma once

#include "core.h"
#include "core/container/array.h"
#include "core/container/tvector.h"

struct VulkanDesc;

struct QueueFamilyIndices {
	int32_t graphics_family = -1;
	int32_t async_compute_family = -1;
	int32_t async_transfer_family = -1;
	int32_t present_family = -1;

	inline bool is_complete() {
		return graphics_family != -1 && present_family != -1 && async_transfer_family != -1 && async_compute_family != -1;
	}

	inline int32_t operator[](QueueType type) {
		return (&graphics_family)[(int)type];
	}
};

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

struct Device {
	VulkanDesc desc;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;

	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;

	VkPhysicalDeviceFeatures device_features;
	VkPhysicalDeviceLimits device_limits;

	QueueFamilyIndices queue_families;

	VkQueue graphics_queue;
	VkQueue compute_queue;
	VkQueue transfer_queue;
	VkQueue present_queue;

	inline operator VkDevice() { return device; }
	inline operator VkPhysicalDevice() { return physical_device; }
	inline VkQueue operator[](QueueType type) { return (&graphics_queue)[type]; }
};

VkSurfaceKHR make_Device(Device& device, const VulkanDesc& desc, struct Window& window);
void destroy_Device(Device& device);

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	array<20, VkSurfaceFormatKHR> formats;
	array<20, VkPresentModeKHR> present_modes;
};

struct QueueSubmitInfo {
	VkFence completion_fence;
	tvector<VkCommandBuffer> cmd_buffers;
	
	tvector<VkPipelineStageFlags> wait_dst_stages;
	tvector<VkSemaphore> wait_semaphores;
	//tvector<VkSemaphore> wait_timeline_semaphores;
	tvector<VkSemaphore> signal_semaphores;
	//tvector<VkSemaphore> signal_timeline_semaphores;
	
	tvector<u64> wait_semaphores_values;
	tvector<u64> signal_semaphores_values;
};

void queue_wait_semaphore(QueueSubmitInfo&, VkPipelineStageFlags, VkSemaphore);
void queue_wait_timeline_semaphore(QueueSubmitInfo&, VkPipelineStageFlags, VkSemaphore, u64);
void queue_signal_timeline_semaphore(QueueSubmitInfo&, VkSemaphore, u64);
void queue_signal_semaphore(QueueSubmitInfo&, VkSemaphore);
void queue_submit(Device& device, QueueType type, const QueueSubmitInfo&);
void queue_submit(VkDevice device, VkQueue queue, const QueueSubmitInfo&);

VkEvent     make_Event(VkDevice);
VkFence     make_Fence(VkDevice);
VkSemaphore make_timeline_Semaphore(VkDevice);
VkSemaphore make_Semaphore(VkDevice);

SwapChainSupportDetails query_swapchain_support(VkSurfaceKHR surface, VkPhysicalDevice device);
