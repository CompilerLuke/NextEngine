#include "stdafx.h"
#include "graphics/rhi/vulkan/swapchain.h"
#include "graphics/rhi/vulkan/device.h"
#include "graphics/rhi/window.h"
#include <GLFW/glfw3.h>

VkSurfaceFormatKHR choose_swap_surface_format(slice<VkSurfaceFormatKHR> availableFormats) {
	for (int i = 0; i < availableFormats.length; i++) {
		if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormats[i];
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR choose_swap_present_mode(slice<VkPresentModeKHR> availablePresentModes) {
	for (int i = 0; i < availablePresentModes.length; i++) {
		if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentModes[i];
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swap_extent(VkSurfaceCapabilitiesKHR& capabilities, Window& window) {
	if (capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	}

	int width, height;
	window.get_framebuffer_size(&width, &height);
	VkExtent2D actuallExtent = { (uint32_t)width, (uint32_t)height };

	actuallExtent.width = glm::clamp(actuallExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actuallExtent.height = glm::clamp(actuallExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return actuallExtent;
}

VkSurfaceKHR make_Surface(VkInstance instance, Window& window) {
	VkSurfaceKHR surface;
	if (glfwCreateWindowSurface(instance, window.window_ptr, nullptr, &surface) != VK_SUCCESS) {
		throw "failed to make window surface!";
	}
	return surface;
}

void destroy_Surface(VkInstance instance, VkSurfaceKHR surface) {
	vkDestroySurfaceKHR(instance, surface, nullptr);
}

Swapchain make_SwapChain(VkDevice device, VkPhysicalDevice physical_device, Window& window, VkSurfaceKHR surface) {
	Swapchain swap_chain = {};
	swap_chain.surface = surface;

	SwapChainSupportDetails swapChainSupport = query_swapchain_support(surface, physical_device);

	VkSurfaceFormatKHR surfaceFormat = choose_swap_surface_format(swapChainSupport.formats);
	VkPresentModeKHR presentMode = choose_swap_present_mode(swapChainSupport.present_modes);

	VkExtent2D extent = choose_swap_extent(swapChainSupport.capabilities, window);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR makeInfo = {};
	makeInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	makeInfo.surface = surface;

	makeInfo.minImageCount = imageCount;
	makeInfo.imageFormat = surfaceFormat.format;
	makeInfo.imageColorSpace = surfaceFormat.colorSpace;
	makeInfo.imageExtent = extent;
	makeInfo.imageArrayLayers = 1;
	makeInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = find_queue_families(physical_device, surface);
	uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphics_family, (uint32_t)indices.present_family };

	if (indices.graphics_family != indices.present_family) {
		makeInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		makeInfo.queueFamilyIndexCount = 2;
		makeInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		makeInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		makeInfo.queueFamilyIndexCount = 0;
		makeInfo.pQueueFamilyIndices = nullptr;
	}

	makeInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	makeInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	makeInfo.presentMode = presentMode;
	makeInfo.clipped = VK_TRUE;

	makeInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device, &makeInfo, nullptr, &swap_chain.swapchain) != VK_SUCCESS) {
		throw "failed to make swap chain!";
	}

	vkGetSwapchainImagesKHR(device, swap_chain, &swap_chain.images.length, nullptr);
	swap_chain.images.resize(swap_chain.images.length);
	vkGetSwapchainImagesKHR(device, swap_chain, &swap_chain.images.length, swap_chain.images.data);

	swap_chain.imageFormat = surfaceFormat.format;
	swap_chain.extent = extent;

	return swap_chain;
}

void destroy_sync_objects(VkDevice device, Swapchain& swapchain) {
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, swapchain.image_available_semaphore[i], nullptr);
		vkDestroySemaphore(device, swapchain.render_finished_semaphore[i], nullptr);
		vkDestroyFence(device, swapchain.in_flight_fences[i], nullptr);
	}
}