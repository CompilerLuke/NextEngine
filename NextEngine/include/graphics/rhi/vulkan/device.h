#pragma once

#include "vulkan.h"
#include "core/container/array.h"

struct VulkanDesc;

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

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	array<20, VkSurfaceFormatKHR> formats;
	array<20, VkPresentModeKHR> present_modes;
};

struct QueueFamilyIndices {
	int32_t graphics_family;
	int32_t present_family;

	bool is_complete() {
		return graphics_family != -1 && present_family != -1;
	}
};

void setup_debug_messenger(VkInstance instance, const VulkanDesc& desc, VkDebugUtilsMessengerEXT* result);
void make_logical_devices(Device& device, const VulkanDesc& desc, VkSurfaceKHR surface);
VkPhysicalDevice pick_physical_devices(VkInstance instance, VkSurfaceKHR surface);
VkInstance make_Instance(const VulkanDesc& desc);
SwapChainSupportDetails query_swapchain_support(VkSurfaceKHR surface, VkPhysicalDevice device);
int32_t find_graphics_queue_family(VkPhysicalDevice device);
int32_t find_present_queue_family(VkPhysicalDevice device, VkSurfaceKHR surface);
QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);
void destroy_Instance(VkInstance instance);
void destroy_validation_layers(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger);