#include "stdafx.h"
#include "graphics/rhi/vulkan/device.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#ifdef RENDER_API_VULKAN

bool check_validation_layer_support(const VulkanDesc& desc) {
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

const char* device_extensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

array<20, VkQueueFamilyProperties> queue_family_properties(VkPhysicalDevice device) {
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

	array<20, VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data);

	return queue_families;
}

int32_t find_graphics_queue_family(VkPhysicalDevice device) {
	auto queue_families = queue_family_properties(device);

	for (int i = 0; i < queue_families.length; i++) {
		VkQueueFamilyProperties& queue_family = queue_families[i];
		if (queue_family.queueCount > 0 && queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			return i;
		}
	}

	return -1;
}

int32_t find_present_queue_family(VkPhysicalDevice device, VkSurfaceKHR surface) {
	auto queue_families = queue_family_properties(device);

	for (int i = 0; i < queue_families.length; i++) {
		VkQueueFamilyProperties& queue_family = queue_families[i];
		
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

		if (present_support) return i;
	}

	return -1;
}

QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
	int32_t graphics_family = find_graphics_queue_family(device);
	int32_t present_family = find_present_queue_family(device, surface);

	return { graphics_family, present_family };
}

SwapChainSupportDetails query_swapchain_support(VkSurfaceKHR surface, VkPhysicalDevice device) {
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

bool check_device_extension_support(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	array<100, VkExtensionProperties> availableExtensions(extensionCount);

	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data);

	for (int i = 0; i < sizeof(device_extensions) / sizeof(char*); i++) {
		const char* name = device_extensions[i];

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

bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
	int32_t graphics_family = find_graphics_queue_family(device);
	int32_t present_family = find_present_queue_family(device, surface);
	bool is_complete = graphics_family != -1 && present_family != -1;

	bool extensionSupport = check_device_extension_support(device);

	bool swapChainAdequate = false;
	if (extensionSupport) {
		SwapChainSupportDetails swapChainSupport = query_swapchain_support(surface, device);
		swapChainAdequate = swapChainSupport.formats.length != 0 && swapChainSupport.present_modes.length != 0;
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return is_complete && extensionSupport && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

uint32_t rate_device_suitablity(VkPhysicalDevice device, VkSurfaceKHR surface) {
	if (!is_device_suitable(device, surface)) return 0;

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

array<20, const char*> get_required_extensions(const VulkanDesc& desc) {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	array<20, const char*> extensions(glfwExtensions, glfwExtensionCount);

	if (desc.num_validation_layers > 0) {
		extensions.append(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData

) {
	VulkanDesc* desc = (VulkanDesc*)pUserData;

	if (messageSeverity >= desc->min_log_severity) {
		printf("validation layers: %s", pCallbackData->pMessage);
	}

	return VK_FALSE;
}


void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& makeInfo, const VulkanDesc& desc) {
	makeInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	makeInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	makeInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	makeInfo.pfnUserCallback = debug_callback;
	makeInfo.pUserData = new VulkanDesc(desc); //todo questionable use
	makeInfo.flags = 0;
}

void setup_debug_messenger(VkInstance instance, const VulkanDesc& desc, VkDebugUtilsMessengerEXT* result) {
	if (desc.num_validation_layers == 0) return;

	VkDebugUtilsMessengerCreateInfoEXT makeInfo;
	populate_debug_messenger_create_info(makeInfo, desc);

	if (vkCreateDebugUtilsMessengerEXT(instance, &makeInfo, nullptr, result) != VK_SUCCESS) {
		throw "failed to setup debug messenger!";
	}
}

VkInstance make_Instance(const VulkanDesc& desc) {
	if (!check_validation_layer_support(desc)) {
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
	auto required_extensions = get_required_extensions(desc);
	makeInfo.enabledExtensionCount = required_extensions.length;
	makeInfo.ppEnabledExtensionNames = required_extensions.data;

	//Enable validation layers
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};

	makeInfo.enabledLayerCount = desc.num_validation_layers;
	makeInfo.pNext = NULL;

	if (desc.num_validation_layers > 0) {
		makeInfo.ppEnabledLayerNames = desc.validation_layers;

		populate_debug_messenger_create_info(debugCreateInfo, desc);
		makeInfo.pNext = &debugCreateInfo;
	}

	VkInstance result;

	if (vkCreateInstance(&makeInfo, nullptr, &result) != VK_SUCCESS) {
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

	return result;
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
		uint32_t current_score = rate_device_suitablity(devices[i], surface);
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

void make_logical_devices(Device& device, const VulkanDesc& desc, VkSurfaceKHR surface) {
	VkPhysicalDevice physical_device = device.physical_device;

	QueueFamilyIndices indices = find_queue_families(physical_device, surface);

	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = indices.graphics_family;
	queueCreateInfo.queueCount = 1;

	float queuePriority = 1.0f;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkDeviceCreateInfo makeInfo = {};
	makeInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	makeInfo.pQueueCreateInfos = &queueCreateInfo;
	makeInfo.queueCreateInfoCount = 1;

	makeInfo.pEnabledFeatures = &desc.device_features;

	makeInfo.enabledExtensionCount = sizeof(device_extensions) / sizeof(const char*);
	makeInfo.ppEnabledExtensionNames = device_extensions;

	makeInfo.enabledLayerCount = desc.num_validation_layers;
	if (desc.num_validation_layers > 0) {
		makeInfo.ppEnabledLayerNames = desc.validation_layers;
	}

	if (vkCreateDevice(physical_device, &makeInfo, nullptr, &device.device) != VK_SUCCESS) {
		throw "failed to make logical device!";
	}

	vkGetDeviceQueue(device, indices.graphics_family, 0, &device.graphics_queue);

	array<20, VkDeviceQueueCreateInfo> queueCreateInfos;

	array<2, uint32_t> uniqueQueueFamilies(2);
	uniqueQueueFamilies[0] = (uint32_t)indices.graphics_family;
	uniqueQueueFamilies[1] = (uint32_t)indices.present_family;

	if (indices.graphics_family == indices.present_family) uniqueQueueFamilies.length = 1;

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

	vkGetDeviceQueue(device, indices.present_family, 0, &device.present_queue);
}

void destroy_Instance(VkInstance instance) {
	vkDestroyInstance(instance, nullptr);
}

void destroy_validation_layers(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger) {
	if (debug_messenger) {
		vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
	}
}


#endif