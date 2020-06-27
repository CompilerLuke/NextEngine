#pragma once

#include "core.h"
#include "core/container/array.h"

struct Window;
struct Device;

const int MAX_SWAPCHAIN_IMAGES = 4;

struct Swapchain {
	VkDevice device;
	VkInstance instance;
	Window* window;
	VkSurfaceKHR surface;
	
	VkSwapchainKHR swapchain;
	array<10, VkImage> images;
	VkFormat imageFormat;
	VkExtent2D extent;
	array<MAX_SWAPCHAIN_IMAGES, VkImageView> image_views;
	array<MAX_SWAPCHAIN_IMAGES, VkFramebuffer> framebuffers;

	//Synchronization
	size_t current_frame = 0;
	uint32_t image_index;
	VkSemaphore image_available_semaphore[MAX_SWAPCHAIN_IMAGES];
	VkSemaphore render_finished_semaphore[MAX_SWAPCHAIN_IMAGES];
	VkFence in_flight_fences[MAX_SWAPCHAIN_IMAGES];
	array<10, VkFence> images_in_flight;

	operator VkSwapchainKHR() {
		return swapchain;
	}
};

void make_Swapchain(Swapchain& swapchain, Device& device, Window& window, VkSurfaceKHR surface);
void make_SyncObjects(Swapchain& swapchain);
VkSurfaceKHR make_Surface(VkInstance instance, Window& window);
void present_swapchain_image(Swapchain& swapchain);
void destroy_Swapchain(Swapchain& swapchain);
