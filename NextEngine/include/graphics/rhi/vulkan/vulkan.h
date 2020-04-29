#pragma once

#include "core.h"
#include <glm/mat4x4.hpp>

struct RHI;
struct BufferAllocator;
struct TextureAllocator;
struct CommandPool;
struct StagingQueue;

VkDevice get_Device(RHI&);
VkPhysicalDevice get_PhysicalDevice(RHI&);
VkInstance get_Instance(RHI&);
BufferAllocator& get_BufferAllocator(RHI&);
TextureAllocator& get_TextureAllocator(RHI&);
VkDescriptorPool get_DescriptorPool(RHI&);
CommandPool& get_CommandPool(RHI&, QueueType queue_type);
VkQueue get_Queue(RHI&, QueueType queue_type);
uint get_frames_in_flight(RHI&);
uint get_active_frame(RHI&);
StagingQueue& get_StagingQueue(RHI&);

struct Window;
struct Level;
struct ModelManager;
struct RHI;

struct VulkanDesc {
	const char* app_name = "No name";
	const char* engine_name = "No name";

	uint32_t engine_version;
	uint32_t app_version;
	uint32_t api_version;

	uint32_t min_log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	uint32_t num_validation_layers = 0;
	const char** validation_layers;

	VkPhysicalDeviceFeatures device_features;
};

struct FrameData {
	glm::mat4 model_matrix;
	glm::mat4 view_matrix;
	glm::mat4 proj_matrix;
};

//todo abstract VulkanDesc
struct Assets;

RHI* make_RHI(const VulkanDesc&, Window&);
void init_RHI(RHI* rhi, Assets&);

void vk_draw_frame(RHI& rhi, FrameData& data);
void destroy_RHI(RHI* rhi);