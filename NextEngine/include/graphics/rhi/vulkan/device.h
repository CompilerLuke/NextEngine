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

struct Device {
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;

	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;

	VkPhysicalDeviceFeatures device_features;

	QueueFamilyIndices queue_families;

	VkQueue graphics_queue;
	VkQueue compute_queue;
	VkQueue transfer_queue;
	VkQueue present_queue;

	operator VkDevice() {
		return device;
	}

	inline VkQueue operator[](QueueType type) {
		return (&graphics_queue)[type];
	}
};

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
	tvector<VkSemaphore> signal_semaphores;
	
	tvector<u64> wait_semaphores_values;
	tvector<u64> signal_semaphores_values;
};

void queue_wait_semaphore(QueueSubmitInfo& info, VkPipelineStageFlags, VkSemaphore);
void queue_wait_timeline_semaphore(QueueSubmitInfo& info, VkPipelineStageFlags, VkSemaphore, u64);
void queue_signal_timeline_semaphore(QueueSubmitInfo& info, VkSemaphore, u64);
void queue_signal_semaphore(QueueSubmitInfo& info, VkSemaphore);
void queue_submit(Device& device, QueueType type, const QueueSubmitInfo&);
void queue_submit(VkDevice device, VkQueue queue, const QueueSubmitInfo&);

VkFence make_Fence(VkDevice);
VkSemaphore make_timeline_Semaphore(VkDevice);
VkSemaphore make_Semaphore(VkDevice);

void setup_debug_messenger(VkInstance instance, const VulkanDesc& desc, VkDebugUtilsMessengerEXT* result);
void make_logical_devices(Device& device, const VulkanDesc& desc, VkSurfaceKHR surface);
VkPhysicalDevice pick_physical_devices(VkInstance instance, VkSurfaceKHR surface);
VkInstance make_Instance(const VulkanDesc& desc);
SwapChainSupportDetails query_swapchain_support(VkSurfaceKHR surface, VkPhysicalDevice device);
void destroy_Instance(VkInstance instance);
void destroy_validation_layers(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger);