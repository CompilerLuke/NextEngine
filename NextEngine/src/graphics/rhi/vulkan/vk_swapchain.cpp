#ifdef RENDER_API_VULKAN

#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/window.h"
#include <GLFW/glfw3native.h>

VkSurfaceFormatKHR choose_swap_surface_format(slice<VkSurfaceFormatKHR> availableFormats) {
	for (int i = 0; i < availableFormats.length; i++) {
		if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormats[i];
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR choose_swap_present_mode(slice<VkPresentModeKHR> availablePresentModes, bool vsync) {
	if (vsync) return VK_PRESENT_MODE_FIFO_KHR;
	
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

void make_SyncObjects(Swapchain& swapchain) {
	VkDevice device = swapchain.device;
	
	for (int i = 0; i < swapchain.images.length; i++) swapchain.images_in_flight.append((VkFence)VK_NULL_HANDLE);

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &swapchain.image_available_semaphore[i]) != VK_SUCCESS
			|| vkCreateSemaphore(device, &semaphoreInfo, nullptr, &swapchain.render_finished_semaphore[i]) != VK_SUCCESS
			|| vkCreateSemaphore(device, &semaphoreInfo, nullptr, &swapchain.render_finished_semaphore2[i]) != VK_SUCCESS
			|| vkCreateFence(device, &fenceInfo, nullptr, &swapchain.in_flight_fences[i]) != VK_SUCCESS) {
			throw "failed to make semaphore!";
		}
	}
}

void make_Swapchain(Swapchain& swapchain, Device& device, Window& window, VkSurfaceKHR surface) {
	swapchain.window = &window;
	swapchain.instance = device.instance;
	swapchain.device = device;
	swapchain.surface = surface;

	SwapChainSupportDetails swapChainSupport = query_swapchain_support(surface, device.physical_device);

	VkSurfaceFormatKHR surfaceFormat = choose_swap_surface_format(swapChainSupport.formats);
	VkPresentModeKHR presentMode = choose_swap_present_mode(swapChainSupport.present_modes, window.vSync);

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

	const QueueFamilyIndices& indices = device.queue_families;
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

	makeInfo.oldSwapchain =  swapchain.swapchain;

	if (vkCreateSwapchainKHR(device, &makeInfo, nullptr, &swapchain.swapchain) != VK_SUCCESS) {
		throw "failed to make swap chain!";
	}

	//MAKE IMAGES
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain.images.length, nullptr);
	swapchain.images.resize(swapchain.images.length);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain.images.length, swapchain.images.data);

	swapchain.imageFormat = surfaceFormat.format;
	swapchain.extent = extent;

	//MAKE IMAGE VIEWS
	swapchain.image_views.resize(swapchain.images.length);

	for (int i = 0; i < swapchain.images.length; i++) {
		swapchain.image_views[i] = make_ImageView(device, swapchain.images[i], swapchain.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void destroy_Swapchain(Swapchain& swapchain) {
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(swapchain.device, swapchain.image_available_semaphore[i], nullptr);
		vkDestroySemaphore(swapchain.device, swapchain.render_finished_semaphore[i], nullptr);
		vkDestroySemaphore(swapchain.device, swapchain.render_finished_semaphore2[i], nullptr);
		vkDestroyFence(swapchain.device, swapchain.in_flight_fences[i], nullptr);
	}

	vkDestroySurfaceKHR(swapchain.instance, swapchain.surface, nullptr);
	vkDestroySwapchainKHR(swapchain.device, swapchain.swapchain, nullptr);
}

#endif