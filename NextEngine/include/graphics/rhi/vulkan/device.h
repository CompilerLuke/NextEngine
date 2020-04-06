#pragma once

#include "vulkan.h"
#include "core/container/array.h"

struct Device {
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;

	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;

	VkPhysicalDeviceFeatures device_features;

	VkQueue present_queue;
	VkQueue graphics_queue;

	operator VkDevice() {
		return device;
	}
};

struct QueueFamilyIndices {
	int32_t graphicsFamily = -1;
	int32_t presentFamily = -1;

	inline bool is_complete() {
		return graphicsFamily != -1 && presentFamily != -1;
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	array<20, VkSurfaceFormatKHR> formats;
	array<20, VkPresentModeKHR> present_modes;
};

void setup_debug_messenger(VkInstance instance, const VulkanDesc& desc, VkDebugUtilsMessengerEXT* result);
void make_logical_devices(Device& device, const VulkanDesc& desc, VkSurfaceKHR surface);
VkPhysicalDevice pick_physical_devices(VkInstance instance, VkSurfaceKHR surface);
VkInstance make_Instance(const VulkanDesc& desc);
SwapChainSupportDetails query_swapchain_support(VkSurfaceKHR surface, VkPhysicalDevice device);
QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);
void destroy_Instance(VkInstance instance);
void destroy_validation_layers(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger);