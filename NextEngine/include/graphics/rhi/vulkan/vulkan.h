#pragma once

#include <glm/mat4x4.hpp>
#include "core.h"
#include "device.h"
#include "swapchain.h"
#include "buffer.h"
#include "pipeline.h"
#include "texture.h"
#include "command_buffer.h"
#include "shader_access.h"
#include "material.h"

struct RenderThreadResources {
	CommandPool command_pool;
	InstanceAllocator instance_allocator;
};


struct Task {
	void* data;
	void(*func)(void*);
};

struct RHI {
	VulkanDesc desc;
	Device device;
	Swapchain swapchain;
	Window* window;
	
	shaderc_compiler_t shader_compiler;
	VertexLayouts vertex_layouts;
	VertexStreaming vertex_streaming;
	TextureAllocator texture_allocator;
	CommandPool background_graphics;

	UBOAllocator ubo_allocator;
	PipelineCache pipeline_cache;
	StagingQueue staging_queue;
	VkCommandPool transfer_cmd_pool;
	DescriptorPool descriptor_pool;
	MaterialAllocator material_allocator;

	uint waiting_on_transfer_frame;
	uint frame_index;

	vector<Task> queued_for_destruction[MAX_FRAMES_IN_FLIGHT];

	RHI();
};

extern thread_local RenderThreadResources render_thread;
extern RHI rhi;

ENGINE_API void begin_gpu_upload();
ENGINE_API void end_gpu_upload();
ENGINE_API void wait_on_transfer(uint frame, VkCommandBuffer cmd_buffer, QueueSubmitInfo& info);

/*
VkDevice get_Device(RHI&);
VkPhysicalDevice get_PhysicalDevice(RHI&);
VkInstance get_Instance(RHI&);
BufferAllocator& get_BufferAllocator(RHI&);
TextureAllocator& get_TextureAllocator(RHI&);
VkDescriptorPool get_DescriptorPool(RHI&);
PipelineCache& get_PipelineCache(RHI& rhi);
CommandPool& get_CommandPool(RHI&, QueueType queue_type);
VkQueue get_Queue(RHI&, QueueType queue_type);
uint get_frames_in_flight(RHI&);
uint get_active_frame(RHI&);
StagingQueue& get_StagingQueue(RHI&);
Assets& get_Assets(RHI&);
*/

struct Window;
struct Level;
struct ModelManager;
struct RHI;


//todo abstract VulkanDesc
struct Assets;

void make_RHI(const VulkanDesc&, Window&);
void init_RHI();
void submit_frame_RHI();
void destroy_RHI();