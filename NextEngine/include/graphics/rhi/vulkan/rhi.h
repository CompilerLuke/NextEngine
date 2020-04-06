#pragma once

#include "vulkan.h"
#include "device.h"
#include "swapchain.h"

struct RHI {
	struct Window& window;
	VulkanDesc desc;
	Device device;
	Swapchain swapchain;
	VkQueue graphics_queue;
	VkQueue present_queue;
};