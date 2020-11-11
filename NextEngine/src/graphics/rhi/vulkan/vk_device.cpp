#ifdef RENDER_API_VULKAN

#include "graphics/rhi/vulkan/device.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include <string.h>
#include <stdio.h>
#include <GLFW/glfw3.h>

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

QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
	QueueFamilyIndices indices;

	//GET QUEUE FAMILY PROPERTIES
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

	array<20, VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data);

	printf("\nQUEUE FAMILY SELECTION\n");

	uint best_transfer_ranking = 0;
	uint best_compute_ranking = 0;

	//FILTER QUEUES
	for (int i = 0; i < queue_families.length; i++) {
		VkQueueFamilyProperties& queue_family = queue_families[i];

		if (queue_family.queueCount == 0) continue;

		bool graphics_support = queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT;
		bool transfer_support = queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT;
		bool compute_support  = queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT;

		uint transfer_ranking = graphics_support ? 1 : compute_support ? 2 : 3;
		uint compute_ranking = graphics_support ? 1 : 2;
		
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

		if (indices.graphics_family == -1 && graphics_support) {
			indices.graphics_family = i;
		}

		if (transfer_support && transfer_ranking > best_transfer_ranking) {
			indices.async_transfer_family = i;
			best_transfer_ranking = transfer_ranking;
		}

		if (compute_support && compute_ranking > best_compute_ranking) {
			indices.async_compute_family = i;
			best_compute_ranking = compute_ranking;
		}

		if (indices.present_family == -1 && present_support) {
			indices.present_family = i;
		}

		printf("\tQUEUE INDEX %i (%2.i) graphics %d transfer %d compute %d present %d\n", i, queue_family.queueCount, graphics_support, transfer_support, compute_support, present_support);

	}

	printf("\tSELECTED graphics %i", indices.graphics_family);
	printf("\tSELECTED transfer %i", indices.async_transfer_family);
	printf("\tSELECTED compute  %i", indices.async_compute_family);
	printf("\tSELECTED present  %i", indices.present_family);

	//todo Assumes that a graphics capable family can perform all other operations as well
	//in theory this should cover most graphics cards, but there may be an exception which 
	//will then crash without warning

	
	return indices;
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

#include "core/container/vector.h"

bool check_device_extension_support(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	vector<VkExtensionProperties> availableExtensions;
	availableExtensions.resize(extensionCount);

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
	QueueFamilyIndices indices = find_queue_families(device, surface);

	bool is_complete = indices.is_complete();

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

	VkDebugUtilsMessengerCreateInfoEXT makeInfo = {};
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
	VkValidationFeaturesEXT validationFeaturesCreateInfo = {};
	array<5, VkValidationFeatureEnableEXT> validation_feature_enable = {};

	makeInfo.enabledLayerCount = desc.num_validation_layers;
	makeInfo.pNext = NULL;

	if (desc.num_validation_layers > 0) {
		makeInfo.ppEnabledLayerNames = desc.validation_layers;

		populate_debug_messenger_create_info(debugCreateInfo, desc);
		makeInfo.pNext = &debugCreateInfo;

		validation_feature_enable.append(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
	
		validationFeaturesCreateInfo.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
		validationFeaturesCreateInfo.enabledValidationFeatureCount = validation_feature_enable.length;
		validationFeaturesCreateInfo.pEnabledValidationFeatures = validation_feature_enable.data;

		debugCreateInfo.pNext = &validationFeaturesCreateInfo;
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

void make_logical_device(Device& device, const VulkanDesc& desc, VkSurfaceKHR surface) {	
	QueueFamilyIndices queue_families = find_queue_families(device.physical_device, surface);
	device.queue_families = queue_families;

	//VkPhysicalDeviceTimelineSemaphoreFeatures semaphore_features = {};
	//semaphore_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
	//semaphore_features.timelineSemaphore = true;

	//VkPhysicalDeviceFeatures2 physical_device_features_2 = {};
	//physical_device_features_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	//physical_device_features_2.features = desc.device_features;
	//physical_device_features_2.pNext = &physical_device_features_12;

	VkPhysicalDeviceVulkan12Features physical_device_features_12 = {};
	physical_device_features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	physical_device_features_12.timelineSemaphore = true;


	VkDeviceCreateInfo makeInfo = {};
	makeInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	makeInfo.enabledExtensionCount = sizeof(device_extensions) / sizeof(const char*);
	makeInfo.ppEnabledExtensionNames = device_extensions;
	makeInfo.pEnabledFeatures = &desc.device_features;
	makeInfo.pNext = &physical_device_features_12;


	makeInfo.enabledLayerCount = desc.num_validation_layers;
	if (desc.num_validation_layers > 0) {
		makeInfo.ppEnabledLayerNames = desc.validation_layers;
	}

	//DEFINE ALL QUEUES TO BE CREATED
	array<4, uint32_t> uniqueQueueFamilies;

	if (!uniqueQueueFamilies.contains(queue_families.graphics_family)) uniqueQueueFamilies.append(queue_families.graphics_family);
	if (!uniqueQueueFamilies.contains(queue_families.async_compute_family)) uniqueQueueFamilies.append(queue_families.async_compute_family);
	if (!uniqueQueueFamilies.contains(queue_families.async_transfer_family)) uniqueQueueFamilies.append(queue_families.async_transfer_family);
	if (!uniqueQueueFamilies.contains(queue_families.present_family)) uniqueQueueFamilies.append(queue_families.present_family);

	array<4, VkDeviceQueueCreateInfo> queueCreateInfos(uniqueQueueFamilies.length);

	float queuePriority = 1.0f;
	for (uint i = 0; i < uniqueQueueFamilies.length; i++) {
		VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos[i];
		queueCreateInfo = {};

		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = uniqueQueueFamilies[i];
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
	}

	makeInfo.queueCreateInfoCount = queueCreateInfos.length;
	makeInfo.pQueueCreateInfos = queueCreateInfos.data;
	
	if (vkCreateDevice(device.physical_device, &makeInfo, nullptr, &device.device) != VK_SUCCESS) {
		throw "failed to make logical device!";
	}

	VkPhysicalDeviceTimelineSemaphoreProperties semaphore_properties = {};
	semaphore_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES;

	VkPhysicalDeviceVulkan12Properties properties_12 = {};
	properties_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
	properties_12.pNext = &semaphore_properties;

	VkPhysicalDeviceProperties2 properties = {};
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &semaphore_properties;

	vkGetPhysicalDeviceProperties2(device.physical_device, &properties);
	
	device.device_limits = properties.properties.limits;
	
	printf("\n\nSEMAPHORE MAX VALUE DIFFERENCE %i\n", properties_12.maxTimelineSemaphoreValueDifference);
	printf("\n\nSEMAPHORE MAX VALUE DIFFERENCE %i\n", semaphore_properties.maxTimelineSemaphoreValueDifference);

	vkGetDeviceQueue(device, queue_families.graphics_family, 0, &device.graphics_queue);
	vkGetDeviceQueue(device, queue_families.present_family, 0, &device.present_queue);
	vkGetDeviceQueue(device, queue_families.async_compute_family, 0, &device.compute_queue);
	vkGetDeviceQueue(device, queue_families.async_transfer_family, 0, &device.transfer_queue);
}

void destroy_Instance(VkInstance instance) {
	vkDestroyInstance(instance, nullptr);
}

void destroy_validation_layers(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger) {
	if (debug_messenger) {
		vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
	}
}

void destroy_Device(Device& device) {
	destroy_validation_layers(device.instance, device.debug_messenger);
	destroy_Instance(device.instance);
	vkDestroyDevice(device.device, nullptr);
}

VkSurfaceKHR make_Device(Device& device, const VulkanDesc& desc, Window& window) {
	device.desc = desc;

	device.instance = make_Instance(desc);
	volkLoadInstance(device.instance);
	setup_debug_messenger(device.instance, desc, &device.debug_messenger);
	VkSurfaceKHR surface = make_Surface(device.instance, window);

	device.physical_device = pick_physical_devices(device.instance, surface);
	make_logical_device(device, desc, surface);

	volkLoadDevice(device);

	return surface;
}
	
void make_instance(Device& device) {
	device.instance = make_Instance(device.desc);
	volkLoadInstance(device.instance);
	setup_debug_messenger(device.instance, device.desc, &device.debug_messenger);
}


void pick_physical_device(Device& device, VkSurfaceKHR surface) {
	device.physical_device = pick_physical_devices(device.instance, surface);
	make_logical_device(device, device.desc, surface);

	volkLoadDevice(device);
}


void destroy_instance(Device& device) {
	vkDestroyInstance(device.instance, nullptr);
}

void queue_wait_semaphore(QueueSubmitInfo& info, VkPipelineStageFlags stage, VkSemaphore semaphore) {
	info.wait_dst_stages.append(stage);
	info.wait_semaphores.append(semaphore);
	info.wait_semaphores_values.append(0);
}

void queue_wait_timeline_semaphore(QueueSubmitInfo& info, VkPipelineStageFlags stage, VkSemaphore semaphore, u64 value) {
	info.wait_dst_stages.append(stage);
	info.wait_semaphores.append(semaphore);
	info.wait_semaphores_values.append(value);
}

void queue_signal_timeline_semaphore(QueueSubmitInfo& info, VkSemaphore semaphore, u64 value) {
	info.signal_semaphores.append(semaphore);
	info.signal_semaphores_values.append(value);
}

void queue_signal_semaphore(QueueSubmitInfo& info, VkSemaphore semaphore) {
	info.signal_semaphores.append(semaphore);
	info.signal_semaphores_values.append(0);
}

void queue_submit(Device& device, QueueType type, const QueueSubmitInfo& info) {
	queue_submit(device, device[type], info);
}

void queue_submit(VkDevice device, VkQueue queue, const QueueSubmitInfo& info) { 
	VkTimelineSemaphoreSubmitInfo semaphore_timeline_info = {};
	semaphore_timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	semaphore_timeline_info.waitSemaphoreValueCount = info.wait_semaphores.length;
	semaphore_timeline_info.pWaitSemaphoreValues = info.wait_semaphores_values.data;
	semaphore_timeline_info.pSignalSemaphoreValues = info.signal_semaphores_values.data;
	semaphore_timeline_info.signalSemaphoreValueCount = info.signal_semaphores_values.length;
	
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	
	submitInfo.waitSemaphoreCount = info.wait_semaphores.length;
	submitInfo.pWaitSemaphores = info.wait_semaphores.data;
	submitInfo.pWaitDstStageMask = info.wait_dst_stages.data;
	submitInfo.commandBufferCount = info.cmd_buffers.length;
	submitInfo.pCommandBuffers = info.cmd_buffers.data;
	submitInfo.signalSemaphoreCount = info.signal_semaphores.length;
	submitInfo.pSignalSemaphores = info.signal_semaphores.data;
	submitInfo.pNext = &semaphore_timeline_info;

	if (vkQueueSubmit(queue, 1, &submitInfo, info.completion_fence) != VK_SUCCESS) {
		throw "Failed to submit draw command buffers!";
	}
}

VkEvent make_Event(VkDevice device) {
	VkEventCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	info.flags = 0;

	VkEvent event;
	VK_CHECK(vkCreateEvent(device, &info, nullptr, &event));

	return event;
}

VkSemaphore make_Semaphore(VkDevice device) {
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.flags = 0;

	VkSemaphore semaphore;
	VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &semaphore));
	return semaphore;
}

VkSemaphore make_timeline_Semaphore(VkDevice device) {
	VkSemaphoreTypeCreateInfoKHR ext = {};
	ext.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR;
	ext.initialValue = 0;
	ext.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;

	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.flags = 0;
	info.pNext = &ext;
	
	VkSemaphore semaphore;
	VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &semaphore));
	return semaphore;
}

VkFence make_Fence(VkDevice device) {
	VkFenceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence fence;
	VK_CHECK(vkCreateFence(device, &info, nullptr, &fence));
	return fence;
}

#endif
