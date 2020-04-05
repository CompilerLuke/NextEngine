#pragma once

#include "vulkan.h"

struct Device {
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;

	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;

	VkPhysicalDeviceFeatures device_features;

	void create_instance(const VulkanDesc&);
	void setup_debug_messenger();
	void pick_physical_devices();
	void create_logical_devices();
	void destroy_instances();
	void destroy_validation_layers();

	operator VkDevice() {
		return device;
	}
};