#include "stdafx.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include <stdio.h>
#include "core/container/vector.h"
#include "core/io/vfs.h"
#include "core/memory/linear_allocator.h"

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "stb_image.h"
#include "graphics/rhi/window.h"

#include "graphics/assets/model.h"


const int MAX_FRAMES_IN_FLIGHT = 2;
const int MAX_SWAP_CHAIN_IMAGES = MAX_FRAMES_IN_FLIGHT + 1;

VulkanDesc desc;

struct Device {
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;

	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;

	VkPhysicalDeviceFeatures device_features;

	VkQueue present_queue;
	VkQueue graphics_queue;

	void make_instance(const VulkanDesc&);
	void setup_debug_messenger();
	void pick_physical_devices(VkSurfaceKHR);
	void make_logical_devices(VkSurfaceKHR);
	void destroy_instance();
	void destroy_validation_layers();

	operator VkDevice() {
		return device;
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	array<20, VkSurfaceFormatKHR> formats;
	array<20, VkPresentModeKHR> present_modes;
};

struct SwapChain {
	VkSwapchainKHR swapchain;
	array<10, VkImage> images;
	VkFormat imageFormat;
	VkExtent2D extent;
	array<MAX_SWAP_CHAIN_IMAGES, VkImageView> image_views;
	array<MAX_SWAP_CHAIN_IMAGES, VkFramebuffer> framebuffers;

	//Synchronization
	size_t current_frame = 0;
	uint32_t image_index;
	VkSemaphore image_available_semaphore[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_finished_semaphore[MAX_FRAMES_IN_FLIGHT];
	VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];
	array<10, VkFence> images_in_flight;

	VkSurfaceKHR surface;

	operator VkSwapchainKHR() {
		return swapchain;
	}
};

Window* window;

SwapChain swapChain;

VkDescriptorSetLayout descriptorSetLayout;
VkDescriptorPool descriptorPool;

array<MAX_SWAP_CHAIN_IMAGES, VkDescriptorSet> descriptorSets;

VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;

VkRenderPass renderPass;

VkCommandPool commandPool;
array<MAX_SWAP_CHAIN_IMAGES, VkCommandBuffer> commandBuffers;


/*
struct BufferManager {
VkDeviceMemory block;

};

struct Buffer {
VkBuffer buffer;
};


BufferManager buffer_manager;
*/

//Resources
VkBuffer vertexBuffer;
VkDeviceMemory vertexBufferMemory;
VkBuffer indexBuffer;
VkDeviceMemory indexBufferMemory;

array<MAX_SWAP_CHAIN_IMAGES, VkBuffer> uniformBuffers;
array<MAX_SWAP_CHAIN_IMAGES, VkDeviceMemory> uniformBuffersMemory;

VkImage textureImage;
VkDeviceMemory textureImageMemory;
VkImageView textureImageView;
VkSampler textureSampler;

VkImage depthImage;
VkDeviceMemory depthImageMemory;
VkImageView depthImageView;

Level* level;

bool checkValidationLayerSupport() {
	if (desc.num_validation_layers == 0) return true;

	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	array<20, VkLayerProperties> available_layers(layerCount);
	
	vkEnumerateInstanceLayerProperties(&layerCount, available_layers.data);

	for (uint32_t i = 0; i < desc.num_validation_layers; i++) {
		bool layer_found = false;

		for (int c = 0; c < layerCount; c++) {
			VkLayerProperties& layerProperties = available_layers[c];

			if (strcmp(layerProperties.layerName, desc.validation_layers[i]) == 0) {
				layer_found = true;
				break;
			}
		}

		if (!layer_found) return false;
	}

	return true;
}

array<20, const char*> getRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	array<20, const char*> extensions(glfwExtensions, glfwExtensionCount);

	if (desc.num_validation_layers > 0) {
		extensions.append(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData

) {
	if (messageSeverity >= desc.min_log_severity) {
		printf("validation layers: %s", pCallbackData->pMessage);
	}

	return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& makeInfo) {
	makeInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	makeInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	makeInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	makeInfo.pfnUserCallback = debugCallback;
	makeInfo.pUserData = nullptr;
	makeInfo.flags = 0;
}

void Device::make_instance(const VulkanDesc& desc) {
	if (!checkValidationLayerSupport()) {
		throw "validation errors requested, but not available";
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = desc.app_name;
	appInfo.applicationVersion = desc.app_version;
	appInfo.pEngineName = desc.engine_name;
	appInfo.engineVersion = desc.engine_version;
	appInfo.apiVersion = desc.api_version;

	VkInstanceCreateInfo makeInfo = {};
	makeInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	makeInfo.pApplicationInfo = &appInfo;

	//Check required extensions
	auto required_extensions = getRequiredExtensions();
	makeInfo.enabledExtensionCount = required_extensions.length;
	makeInfo.ppEnabledExtensionNames = required_extensions.data;

	//Enable validation layers
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};

	makeInfo.enabledLayerCount = desc.num_validation_layers;
	makeInfo.pNext = NULL;

	if (desc.num_validation_layers > 0) {
		makeInfo.ppEnabledLayerNames = desc.validation_layers;

		populateDebugMessengerCreateInfo(debugCreateInfo);
		makeInfo.pNext = &debugCreateInfo;
	}


	if (vkCreateInstance(&makeInfo, nullptr, &instance) != VK_SUCCESS) {
		throw "failed to make instance!";
	}

	//Query extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	array<20, VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data);

	printf("available extensions:\n");

	for (int i = 0; i < extensionCount; i++) {
		printf("\t%s\n", extensions[i].extensionName);
	}
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

void Device::setup_debug_messenger() {
	if (desc.num_validation_layers == 0) return;

	VkDebugUtilsMessengerCreateInfoEXT makeInfo;
	populateDebugMessengerCreateInfo(makeInfo);

	if (CreateDebugUtilsMessengerEXT(instance, &makeInfo, nullptr, &debug_messenger) != VK_SUCCESS) {
		throw "failed to setup debug messenger!";
	}
}

struct QueueFamilyIndices {
	int32_t graphicsFamily = -1;
	int32_t presentFamily = -1;

	bool isComplete() {
		return graphicsFamily != -1 && presentFamily != -1;
	}
};

const char* deviceExtensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	array<20, VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data);

	for (int i = 0; i < queueFamilyCount; i++) {
		VkQueueFamilyProperties& queueFamily = queueFamilies[i];
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.isComplete()) {
			break;
		}
	}

	return indices;
}


bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	array<100, VkExtensionProperties> availableExtensions(extensionCount);

	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data);

	for (int i = 0; i < sizeof(deviceExtensions) / sizeof(char*); i++) {
		const char* name = deviceExtensions[i];

		bool found = false;
		for (int c = 0; c < extensionCount; c++) {
			if (strcmp(availableExtensions[c].extensionName, name) == 0) {
				found = true;
				break;
			}
		}

		if (!found) return false;
	}

	return true;
}

SwapChainSupportDetails querySwapChainSupport(VkSurfaceKHR surface, VkPhysicalDevice device) {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formats.length, nullptr);
	details.formats.resize(details.formats.length);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formats.length, details.formats.data);

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.present_modes.length, nullptr);
	details.present_modes.resize(details.present_modes.length);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.present_modes.length, details.present_modes.data);

	return details;
};

VkSurfaceFormatKHR chooseSwapSurfaceFormat(slice<VkSurfaceFormatKHR> availableFormats) {
	for (int i = 0; i < availableFormats.length; i++) {
		if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormats[i];
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(slice<VkPresentModeKHR> availablePresentModes) {
	for (int i = 0; i < availablePresentModes.length; i++) {
		if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentModes[i];
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(VkSurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != UINT32_MAX) {
		return capabilities.currentExtent;
	}

	int width, height;
	window->get_framebuffer_size(&width, &height);
	VkExtent2D actuallExtent = { (uint32_t)width, (uint32_t)height };

	actuallExtent.width = glm::clamp(actuallExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actuallExtent.height = glm::clamp(actuallExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return actuallExtent;
}

bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
	QueueFamilyIndices indices = findQueueFamilies(device, surface);

	bool extensionSupport = checkDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionSupport) {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(surface, device);
		swapChainAdequate = swapChainSupport.formats.length != 0 && swapChainSupport.present_modes.length != 0;
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return indices.isComplete() && extensionSupport && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

uint32_t rateDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR surface) {
	if (!isDeviceSuitable(device, surface)) return 0;

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	uint32_t score = 0;

	if (deviceProperties.deviceType & VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score += 1000;
	}

	score += deviceProperties.limits.maxImageDimension2D;

	return score;
}


VkPhysicalDevice pick_physical_devices(VkInstance instance, VkSurfaceKHR surface) {
	array<20, VkPhysicalDevice> devices;

	vkEnumeratePhysicalDevices(instance, &devices.length, nullptr);
	devices.resize(devices.length);

	if (devices.length == 0) {
		throw "failed to find GPUs with vulkan support";
	}

	vkEnumeratePhysicalDevices(instance, &devices.length, devices.data);


	int index = -1;
	uint32_t highest_score = 0;

	for (int i = 0; i < devices.length; i++) {
		uint32_t current_score = rateDeviceSuitability(devices[i], surface);
		if (current_score > highest_score) {
			index = i;
			highest_score = current_score;
		}
	}

	if (index == -1) {
		throw "failed to find a suitable GPU!";
	}

	return devices[index];
}

void Device::pick_physical_devices(VkSurfaceKHR surface) {
	physical_device = ::pick_physical_devices(instance, surface);
}

void Device::make_logical_devices(VkSurfaceKHR surface) {
	QueueFamilyIndices indices = findQueueFamilies(physical_device, surface);

	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
	queueCreateInfo.queueCount = 1;

	float queuePriority = 1.0f;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	device_features = {};
	device_features.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo makeInfo = {};
	makeInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	makeInfo.pQueueCreateInfos = &queueCreateInfo;
	makeInfo.queueCreateInfoCount = 1;

	makeInfo.pEnabledFeatures = &device_features;

	makeInfo.enabledExtensionCount = sizeof(deviceExtensions) / sizeof(const char*);
	makeInfo.ppEnabledExtensionNames = deviceExtensions;

	makeInfo.enabledLayerCount = desc.num_validation_layers;
	if (desc.num_validation_layers > 0) {
		makeInfo.ppEnabledLayerNames = desc.validation_layers;
	}

	if (vkCreateDevice(physical_device, &makeInfo, nullptr, &device) != VK_SUCCESS) {
		throw "failed to make logical device!";
	}

	vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphics_queue);

	array<20, VkDeviceQueueCreateInfo> queueCreateInfos;

	array<2, uint32_t> uniqueQueueFamilies(2);
	uniqueQueueFamilies[0] = (uint32_t)indices.graphicsFamily;
	uniqueQueueFamilies[1] = (uint32_t)indices.presentFamily;

	if (indices.graphicsFamily == indices.presentFamily) uniqueQueueFamilies.length = 1;

	queuePriority = 1.0f;
	for (int i = 0; i < uniqueQueueFamilies.length; i++) {
		VkDeviceQueueCreateInfo queueCreateInfo;
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = uniqueQueueFamilies[i];
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.append(queueCreateInfo);
	}

	makeInfo.queueCreateInfoCount = queueCreateInfos.length;
	makeInfo.pQueueCreateInfos = queueCreateInfos.data;

	vkGetDeviceQueue(device, indices.presentFamily, 0, &present_queue);
}

VkSurfaceKHR makeSurface(VkInstance instance, GLFWwindow* window) {
	VkSurfaceKHR surface;
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw "failed to make window surface!";
	}
	return surface;
}

void destroy_Surface(VkInstance instance, VkSurfaceKHR surface) {
	vkDestroySurfaceKHR(instance, surface, nullptr);
}

SwapChain make_SwapChain(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	SwapChain swap_chain = {};
	swap_chain.surface = surface;

	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(surface, physical_device);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.present_modes);

	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

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

	QueueFamilyIndices indices = findQueueFamilies(physical_device, surface);
	uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };

	if (indices.graphicsFamily != indices.presentFamily) {
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

VkImageView makeImageView(VkDevice device, VkImage image, VkFormat imageFormat, VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo makeInfo = {}; //todo abstract image view creation
	makeInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	makeInfo.image = image;
	makeInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	makeInfo.format = imageFormat;

	makeInfo.subresourceRange.aspectMask = aspectFlags;
	makeInfo.subresourceRange.baseMipLevel = 0;
	makeInfo.subresourceRange.levelCount = 1;
	makeInfo.subresourceRange.baseArrayLayer = 0;
	makeInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(device, &makeInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw "Failed to make image views!";
	}

	return imageView;
}

void make_image_views(VkDevice device, SwapChain& swapchain) {
	swapchain.image_views.resize(swapchain.images.length);

	for (int i = 0; i < swapchain.images.length; i++) {
		swapchain.image_views[i] = makeImageView(device, swapchain.images[i], swapchain.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

thread_local LinearAllocator linear_allocator(mb(5));

#define VK_CHECK(x)  \
{ \
	VkResult result = x; \
	if (result != VK_SUCCESS) { \
		printf("Vulkan Error %d at %s:%d", result, __FILE__, __LINE__); \
		abort(); \
	} \
}

VkShaderModule makeShaderModule(VkDevice device, string_view code) {
	VkShaderModuleCreateInfo makeInfo = {};
	makeInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	makeInfo.codeSize = code.length;
	makeInfo.pCode = (const unsigned int*)code.data;

	VkShaderModule shaderModule;
	
	VK_CHECK(vkCreateShaderModule(device, &makeInfo, nullptr, &shaderModule))

	return shaderModule;
}


constexpr int num_bindings = 5;
array<num_bindings, VkVertexInputAttributeDescription> Vertex_getAttributeDescriptions() {
	array<num_bindings, VkVertexInputAttributeDescription> attributeDescriptions(num_bindings);

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, position);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, normal);

	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, tex_coord);

	attributeDescriptions[3].binding = 0;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[3].offset = offsetof(Vertex, tangent);

	attributeDescriptions[4].binding = 0;
	attributeDescriptions[4].location = 4;
	attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[4].offset = offsetof(Vertex, bitangent);

	/*vector<VertexAttrib> default_model_layout = {
		{ 3, VertexAttrib::Float, offsetof(Vertex, position) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, normal) },
		{ 2, VertexAttrib::Float, offsetof(Vertex, tex_coord) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, tangent) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, bitangent) }
	};*/

	return attributeDescriptions;
}

VkVertexInputBindingDescription Vertex_getBindingDescriptors() {
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescription;
}

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

slice<Vertex> vertices;
slice<uint> indices;

uint32_t findMemoryType(VkPhysicalDevice physical_device, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) return i;
	}

	throw "Failed to find suitable memory type";
}

void makeBuffer(VkDevice device, VkPhysicalDevice physical_device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw "failed to make buffer!";
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(physical_device, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw "failed to allocate buffer memory!";
	}

	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

struct CmdCopyBuffer {
	VkBuffer srcBuffer;
	VkBuffer dstBuffer;
	VkBufferCopy copyRegion;
};

struct CmdCopyBufferToImage {
	VkBuffer buffer;
	VkImage image;
	VkImageLayout imageLayout;
	VkBufferImageCopy copyRegion;
};

struct CmdPipelineBarrier {
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags dstStage;
	VkImageMemoryBarrier imageBarrier;
};

struct StagingQueue {
	VkCommandBuffer cmd_buffer;
	VkQueue queue;

	//static_vector<10, CmdCopyBuffer> cmdsCopyBuffer;
	//static_vector<10, CmdCopyBufferToImage> cmdsCopyBufferToImage;
	//static_vector<10, CmdPipelineBarrier> cmdsPipelineBarrier;
	array<10, VkBuffer> destroyBuffers;
	array<10, VkDeviceMemory> destroyDeviceMemory;
};

StagingQueue make_StagingQueue(VkDevice device, VkQueue queue) {
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	//TODO make CommandBuffer Pool
	StagingQueue result;
	result.queue = queue;
	vkAllocateCommandBuffers(device, &allocInfo, &result.cmd_buffer);
	return result;
}

void begin_staging_cmds(StagingQueue& queue) {
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(queue.cmd_buffer, &beginInfo);
}

void clear_StagingQueue(StagingQueue& queue) {
	queue.destroyBuffers.clear();
	queue.destroyDeviceMemory.clear();
}

void end_staging_cmds(VkDevice device, StagingQueue& queue) {
	vkEndCommandBuffer(queue.cmd_buffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &queue.cmd_buffer;

	vkQueueSubmit(queue.queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue.queue);

	for (VkBuffer buffer : queue.destroyBuffers) {
		vkDestroyBuffer(device, buffer, nullptr);
	}
	for (VkDeviceMemory memory : queue.destroyDeviceMemory) {
		vkFreeMemory(device, memory, nullptr);
	}
	
	clear_StagingQueue(queue);
}

void destroy_StagingQueue(VkDevice device, StagingQueue& staging) {
	vkFreeCommandBuffers(device, commandPool, 1, &staging.cmd_buffer);
}

StagingQueue staging_queue;

void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;

	vkCmdCopyBuffer(staging_queue.cmd_buffer, srcBuffer, dstBuffer, 1, &copyRegion);
	//transferQueue.cmdsCopyBuffer.append({srcBuffer, dstBuffer, copyRegion});
}

void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0,0,0 };
	region.imageExtent = {
		width, height, 1
	};

	vkCmdCopyBufferToImage(staging_queue.cmd_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	//transferQueue.cmdsCopyBufferToImage.append({buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region});
}


void makeStagedBuffer(VkDevice device, VkPhysicalDevice physical_device, void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	makeBuffer(device, physical_device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, bufferData, (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	makeBuffer(device, physical_device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);

	copyBuffer(stagingBuffer, buffer, bufferSize);

	staging_queue.destroyBuffers.append(stagingBuffer);
	staging_queue.destroyDeviceMemory.append(stagingBufferMemory);

	//vkDestroyBuffer(device, stagingBuffer, nullptr);
	//vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void makeVertexBuffer(VkDevice device, VkPhysicalDevice physical_device) {
	makeStagedBuffer(device, physical_device, (void*)vertices.data, sizeof(vertices[0]) * vertices.length, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexBufferMemory);
}

void makeIndexBuffer(VkDevice device, VkPhysicalDevice physical_device) {
	makeStagedBuffer(device, physical_device, (void*)indices.data, sizeof(indices[0]) * indices.length, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, indexBufferMemory);
}



void makeUniformBuffers(VkDevice device, VkPhysicalDevice physical_device) {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	uniformBuffers.resize(swapChain.images.length);
	uniformBuffersMemory.resize(swapChain.images.length);

	for (int i = 0; i < swapChain.images.length; i++) {
		makeBuffer(device, physical_device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
	}
}

void makeDescriptorPool(VkDevice device) {
	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = swapChain.images.length;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = swapChain.images.length;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = swapChain.images.length;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw "Failed to make descriptor pool!";

	}
}

bool hasStencilComponent(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (hasStencilComponent(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else {
		throw "Unsupported layout transition";
	}

	//vkCmdPipelineBarrier(transferQueue.commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, nullptr, nullptr, 1, &barrier);

	vkCmdPipelineBarrier(
		staging_queue.cmd_buffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	//transferQueue.cmdsPipelineBarrier.append({sourceStage, destinationStage, barrier});
}

void makeImage(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage; //VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw "Could not make image!";
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(physical_device, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw "Could not allocate memory for image!";
	}

	vkBindImageMemory(device, image, imageMemory, 0);
}

void makeTextureImage(VkDevice device, VkPhysicalDevice physical_device) {
	int texWidth = 0, texHeight = 0, texChannels = 0;

	string_buffer tex_path = level->asset_path("wet_street/Pebble_Wet_street_basecolor.jpg");

	void* pixels = stbi_load(tex_path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) throw "Failed to load texture image!";

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	makeBuffer(device, physical_device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, imageSize);
	vkUnmapMemory(device, stagingBufferMemory);

	stbi_image_free(pixels);

	makeImage(device, physical_device, texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, textureImage, texWidth, texHeight);
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	staging_queue.destroyBuffers.append(stagingBuffer);
	staging_queue.destroyDeviceMemory.append(stagingBufferMemory);
}

void makeTextureImageView(VkDevice device) {
	textureImageView = makeImageView(device, textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}

void makeTextureSampler(VkDevice device) {
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
		throw "Failed to make texture sampler!";
	}
}

string_buffer readFileOrFail(string_view src) {
	File file(*level);
	if (!file.open(src, File::ReadFileB)) throw "Failed to load vertex shaders!";
	return file.read();
}

void makeGraphicsPipeline(VkDevice device) {
	size_t before = linear_allocator.occupied;

	string_buffer vertShaderCode = readFileOrFail("shaders/cache/vert.spv");
	string_buffer fragShaderCode = readFileOrFail("shaders/cache/frag.spv");

	VkShaderModule vertShaderModule = makeShaderModule(device, vertShaderCode);
	VkShaderModule fragShaderModule = makeShaderModule(device, fragShaderCode);

	linear_allocator.reset(before);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto bindingDescription = Vertex_getBindingDescriptors();
	auto attributeDescriptions = Vertex_getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;

	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.length;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChain.extent.width;
	viewport.height = (float)swapChain.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChain.extent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 0.0f;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;


	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw "failed to make pipeline!";
	}


	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw "failed to make graphics pipeline!";
	}

	vkDestroyShaderModule(device, vertShaderModule, nullptr);
	vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

VkFormat findDepthFormat(VkPhysicalDevice);

void makeRenderPass(VkDevice device, VkPhysicalDevice physical_device) {
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapChain.imageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = findDepthFormat(physical_device);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw "Failed to make render pass!";
	}

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
}

void makeFramebuffers(VkDevice device, SwapChain& swapchain) {
	swapchain.framebuffers.resize(swapchain.images.length);

	for (int i = 0; i < swapchain.images.length; i++) {
		VkImageView attachments[2] = {
			swapchain.image_views[i],
			depthImageView
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swapchain.extent.width;
		framebufferInfo.height = swapchain.extent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchain.framebuffers[i]) != VK_SUCCESS) {
			throw "failed to make framebuffer!";
		}
	}
}

void makeCommandPool(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physical_device, surface);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = (uint32_t)queueFamilyIndices.graphicsFamily;
	poolInfo.flags = 0;

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw "Could not make pool!";
	}
}

void makeCommandBuffers(VkDevice device, int max_frames) {
	commandBuffers.resize(max_frames);

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = commandPool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = commandBuffers.length;

	if (vkAllocateCommandBuffers(device, &alloc_info, commandBuffers.data) != VK_SUCCESS) {
		throw "Failed to allocate command buffers";
	}

	for (int i = 0; i < commandBuffers.length; i++) {
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = 0;
		begin_info.pInheritanceInfo = nullptr;

		if (vkBeginCommandBuffer(commandBuffers[i], &begin_info) != VK_SUCCESS) {
			throw "Failed to begin recording command buffer!";
		}

		VkRenderPassBeginInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = renderPass;
		render_pass_info.framebuffer = swapChain.framebuffers[i];
		render_pass_info.renderArea.offset = { 0,0 };
		render_pass_info.renderArea.extent = swapChain.extent;

		VkClearValue clear_colors[2];
		clear_colors[0].color = { 0.1f, 0.1f, 0.1f, 1.0f };
		clear_colors[1].depthStencil = { 1, 0 };

		render_pass_info.clearValueCount = 2;
		render_pass_info.pClearValues = clear_colors;

		vkCmdBeginRenderPass(commandBuffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkBuffer vertex_buffers[] = { vertexBuffer };
		VkDeviceSize offsets[] = { 0 };

		vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertex_buffers, offsets);
		vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);

		vkCmdDrawIndexed(commandBuffers[i], indices.length, 1, 0, 0, 0);

		vkCmdEndRenderPass(commandBuffers[i]);

		if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
			throw "failed to record command buffers!";
		}
	}
}

void make_sync_objects(VkDevice device, SwapChain& swapchain) {
	for (int i = 0; i < swapchain.images.length; i++) swapchain.images_in_flight.append((VkFence)VK_NULL_HANDLE);

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &swapchain.image_available_semaphore[i]) != VK_SUCCESS 
		|| vkCreateSemaphore(device, &semaphoreInfo, nullptr, &swapchain.render_finished_semaphore[i]) != VK_SUCCESS 
		|| vkCreateFence(device, &fenceInfo, nullptr, &swapchain.in_flight_fences[i]) != VK_SUCCESS) {
			throw "failed to make semaphore!";
		}
	}
}

void makeDescriptorSetLayout(VkDevice device) {
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding bindings[2] = { uboLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 2;
	layoutInfo.pBindings = bindings;

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw "Failed to make descriptor set layout!";
	}
}

void makeDescriptorSets(VkDevice device) {
	array<MAX_SWAP_CHAIN_IMAGES, VkDescriptorSetLayout> layouts(swapChain.images.length);
	for (int i = 0; i < swapChain.images.length; i++) layouts[i] = descriptorSetLayout;

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChain.images.length);
	allocInfo.pSetLayouts = layouts.data;

	descriptorSets.resize(swapChain.images.length);
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data) != VK_SUCCESS) {
		throw "Failed to make descriptor sets!";
	}

	for (int i = 0; i < swapChain.images.length; i++) {
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureImageView;
		imageInfo.sampler = textureSampler;

		VkWriteDescriptorSet descriptorWrites[2] = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
		descriptorWrites[0].pImageInfo = nullptr;
		descriptorWrites[0].pTexelBufferView = nullptr;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = nullptr;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);
	}
}

VkFormat findSupportedFormat(VkPhysicalDevice physical_device, slice<VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw "Failed to find supported format!";
}

VkFormat findDepthFormat(VkPhysicalDevice physical_device) {
	array<3, VkFormat> candidates = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	return findSupportedFormat(physical_device, candidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void makeDepthResources(VkDevice device, VkPhysicalDevice physical_device) {
	VkFormat depthFormat = findDepthFormat(physical_device);
	makeImage(device, physical_device, swapChain.extent.width, swapChain.extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
	depthImageView = makeImageView(device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	//transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

}

#include "graphics/assets/model.h"
#include "graphics/rhi/primitives.h"

Device device;
VkQueue graphicsQueue;
VkQueue presentQueue;

void vk_init(const VulkanDesc& desc, ModelManager& model_manager, Level& level, Window* window) {
	::desc = desc;
	::window = window;
	::level = &level;

	model_handle handle = model_manager.load("Aset_wood_log_L_rdeu3_LOD0.fbx");
	Model* model = model_manager.get(handle);

	vertices = model->meshes[0].vertices;
	indices = model->meshes[0].indices;

	VK_CHECK(volkInitialize());



	device.make_instance(desc);
	volkLoadInstance(device.instance);
	device.setup_debug_messenger();

	VkSurfaceKHR surface = makeSurface(device.instance, window->window_ptr);

	device.pick_physical_devices(surface);
	device.make_logical_devices(surface);

	graphicsQueue = device.graphics_queue;
	presentQueue = device.present_queue;

	volkLoadDevice(device);
	swapChain = make_SwapChain(device, device.physical_device, surface);
	make_image_views(device, swapChain);
	
	makeRenderPass(device, device.physical_device);
	makeDescriptorSetLayout(device);
	makeGraphicsPipeline(device);
	makeCommandPool(device.device, device.physical_device, surface);
	makeDepthResources(device.device, device.physical_device);
	makeFramebuffers(device, swapChain);
	
	staging_queue = make_StagingQueue(device, graphicsQueue);
	begin_staging_cmds(staging_queue);
	
	makeUniformBuffers(device, device.physical_device);
	makeTextureImage(device, device.physical_device);
	makeTextureImageView(device);
	makeTextureSampler(device);
	makeDescriptorPool(device);
	makeDescriptorSets(device);
	makeVertexBuffer(device, device.physical_device);
	makeIndexBuffer(device, device.physical_device);

	end_staging_cmds(device, staging_queue);
	
	makeCommandBuffers(device, swapChain.framebuffers.length);
	make_sync_objects(device, swapChain);

	::desc.validation_layers = nullptr;
}

void destroy(VkDevice device, SwapChain& swapchain) {
	vkDestroyImageView(device, depthImageView, nullptr);
	vkDestroyImage(device, depthImage, nullptr);
	vkFreeMemory(device, depthImageMemory, nullptr);

	vkFreeCommandBuffers(device, commandPool, commandBuffers.length, commandBuffers.data);

	for (int i = 0; i < swapchain.framebuffers.length; i++) {
		vkDestroyFramebuffer(device, swapchain.framebuffers[i], nullptr);
	}

	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);

	for (int i = 0; i < swapchain.image_views.length; i++) {
		vkDestroyImageView(device, swapchain.image_views[i], nullptr);
		vkDestroyBuffer(device, uniformBuffers[i], nullptr);
		vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(device, descriptorPool, nullptr);

	vkDestroySwapchainKHR(device, swapChain, nullptr);
}

int frames_in_flight(SwapChain& swapchain) {
	return swapchain.framebuffers.length;
}

void remakeSwapChain(VkDevice device, VkPhysicalDevice physical_device, SwapChain& swapchain) {
	int width = 0, height = 0;

	window->get_framebuffer_size(&width, &height);

	while (width == 0 || height == 0) {
		window->get_framebuffer_size(&width, &height);
		window->wait_events();
	}

	vkDeviceWaitIdle(device);
	destroy(device, swapchain);
	swapChain = make_SwapChain(device, physical_device, swapchain.surface);
	make_image_views(device, swapchain);

	makeRenderPass(device, physical_device);
	makeGraphicsPipeline(device);
	makeDepthResources(device, physical_device);
	makeFramebuffers(device, swapchain);
	makeUniformBuffers(device, physical_device);
	makeDescriptorPool(device);
	makeDescriptorSets(device);
	makeCommandBuffers(device, frames_in_flight(swapchain));
}

void Device::destroy_instance() {
	vkDestroyInstance(instance, nullptr);
}

void Device::destroy_validation_layers() {
	if (desc.num_validation_layers > 0) {
		DestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
	}
}

void destroy_sync_objects(VkDevice device, SwapChain& swapchain) {
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, swapchain.image_available_semaphore[i], nullptr);
		vkDestroySemaphore(device, swapchain.render_finished_semaphore[i], nullptr);
		vkDestroyFence(device, swapchain.in_flight_fences[i], nullptr);
	}
}

void vk_deinit() {
	vkDeviceWaitIdle(device);

	destroy_sync_objects(device, swapChain);
	destroy(device, swapChain);

	vkDestroySampler(device, textureSampler, nullptr);
	vkDestroyImageView(device, textureImageView, nullptr);

	vkDestroyImage(device, textureImage, nullptr);
	vkFreeMemory(device, textureImageMemory, nullptr);

	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);

	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);

	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyDevice(device, nullptr);

	device.destroy_validation_layers();

	destroy_Surface(device.instance, swapChain.surface);
	device.destroy_instance();
}

void update_UniformBuffer(uint32_t currentImage, FrameData& frameData) {
	UniformBufferObject ubo = {};
	ubo.model = frameData.model_matrix;
	ubo.view = frameData.view_matrix;
	ubo.proj = frameData.proj_matrix;
	ubo.proj[1][1] *= -1;

	void* data;
	vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(UniformBufferObject), 0, &data);
	memcpy(data, &ubo, sizeof(UniformBufferObject));
	vkUnmapMemory(device, uniformBuffersMemory[currentImage]);

}

void draw_frame(VkDevice device, VkPhysicalDevice physical_device, SwapChain& swapchain, FrameData& frameData) {
	uint current_frame = swapchain.current_frame;
	uint image_index = swapchain.image_index;

	vkWaitForFences(device, 1, &swapchain.in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

	VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, swapchain.image_available_semaphore[current_frame], VK_NULL_HANDLE, &image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		remakeSwapChain(device, physical_device, swapchain);
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw "Failed to acquire swapchain image!";
	}

	if (swapchain.images_in_flight[image_index] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &swapchain.images_in_flight[image_index], VK_TRUE, UINT64_MAX);
	}
	swapchain.images_in_flight[swapchain.image_index] = swapchain.in_flight_fences[swapchain.current_frame];

	update_UniformBuffer(image_index, frameData);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { swapchain.image_available_semaphore[current_frame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[image_index];

	VkSemaphore signalSemaphores[] = { swapchain.render_finished_semaphore[current_frame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(device, 1, &swapchain.in_flight_fences[current_frame]);

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, swapchain.in_flight_fences[current_frame]) != VK_SUCCESS) {
		throw "Failed to submit draw command buffers!";
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &image_index;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		//framebufferResized = false;
		remakeSwapChain(device, physical_device, swapchain);
	}
	else if (result != VK_SUCCESS) {
		throw "failed to present swap chain image!";
	}

	swapchain.current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void vk_draw_frame(FrameData& frameData) {
	draw_frame(device, device.physical_device, swapChain, frameData);
}
