#pragma once

#include "core/container/array.h"
#include "graphics/rhi/buffer.h"
#include "graphics/rhi/vulkan/core.h"

struct BufferAllocator;
struct QueueSubmitInfo;

//StagingQueue is internally sychronized 
//in terms of keeping track of the current frame index
//and which command buffers have executed 
//however it is still invalid to submit commands on different threads.
//Limitations: The current design makes most sense if 
//Dedicated transfer operations 
//command buffer for that specific queue
struct StagingQueue {
	VkDevice device;
	VkPhysicalDevice physical_device;
	VkCommandPool command_pool;
	VkQueue queue;
	int queue_family;
	int dst_queue_family;

	VkCommandBuffer cmd_buffers[MAX_FRAMES_IN_FLIGHT];
	VkFence completed_transfer[MAX_FRAMES_IN_FLIGHT];
	
	VkSemaphore wait_on_transfer;
	VkSemaphore wait_on_resource_in_use;
	uint value_wait_on_transfer;
	uint value_wait_on_resource_in_use;

	uint frame_index;
	bool recording;
};

void transfer_queue_dependencies(StagingQueue&, QueueSubmitInfo&, uint transfer_frame_index, uint frame_index);

struct VertexAttrib {
	enum Kind {
		Float, Int
	};

	unsigned int length;
	Kind kind;
	unsigned int offset;
};

using ArrayVertexInputs = array<10, VkVertexInputAttributeDescription>;
using ArrayVertexBindings = array<2, VkVertexInputBindingDescription>;

StagingQueue make_StagingQueue(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue, int queue_family, int dst_queue_family);
uint begin_staging_cmds(StagingQueue& queue);
void end_staging_cmds(StagingQueue& queue);
void destroy_StagingQueue(StagingQueue& staging);

uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t typeFilter, VkMemoryPropertyFlags properties);
void memcpy_Buffer(VkDevice device, VkDeviceMemory memory, void* buffer_data, u64 buffer_size, u64 offset = 0);
void make_Buffer(VkDevice device, VkPhysicalDevice physical_device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
void copy_Buffer(StagingQueue& staging_queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
//void make_StagedBuffer(StagingQueue& staging_queue, void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

void input_attributes(ArrayVertexInputs& vertex_inputs, slice<VertexAttrib> attribs, int binding);
ArrayVertexInputs input_attributes(BufferAllocator&, VertexLayout);
VkVertexInputBindingDescription input_bindings(BufferAllocator&, VertexLayout);
ArrayVertexInputs input_attributes(BufferAllocator&, VertexLayout, InstanceLayout);
ArrayVertexBindings input_bindings(BufferAllocator&, VertexLayout, InstanceLayout);

void bind_vertex_buffer(BufferAllocator& self, VkCommandBuffer cmd_buffer, VertexLayout v_layout, InstanceLayout layout);
void begin_buffer_upload(BufferAllocator& self);
void end_buffer_upload(BufferAllocator& self);
void transfer_vertex_ownership(BufferAllocator& self, VkCommandBuffer cmd_buffer);