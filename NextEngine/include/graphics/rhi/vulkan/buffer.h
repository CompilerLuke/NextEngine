#pragma once

#include "core/container/array.h"
#include "vulkan.h"

struct BufferManager;

struct StagingQueue {
	VkDevice device;
	VkPhysicalDevice physical_device;
	VkCommandPool command_pool;

	VkCommandBuffer cmd_buffer;
	VkQueue queue;

	//static_vector<10, CmdCopyBuffer> cmdsCopyBuffer;
	//static_vector<10, CmdCopyBufferToImage> cmdsCopyBufferToImage;
	//static_vector<10, CmdPipelineBarrier> cmdsPipelineBarrier;
	array<10, VkBuffer> destroyBuffers;
	array<10, VkDeviceMemory> destroyDeviceMemory;
};

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

StagingQueue make_StagingQueue(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue);
void begin_staging_cmds(StagingQueue& queue);
void end_staging_cmds(VkDevice device, StagingQueue& queue);
void destroy_StagingQueue(StagingQueue& staging);
uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t typeFilter, VkMemoryPropertyFlags properties);
void make_Buffer(VkDevice device, VkPhysicalDevice physical_device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
void copy_Buffer(StagingQueue& staging_queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
void make_StagedBuffer(StagingQueue& staging_queue, void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
void input_attributes(ArrayVertexInputs& vertex_inputs, slice<VertexAttrib> attribs, int binding);
ArrayVertexInputs input_attributes(BufferManager&, VertexLayout);
VkVertexInputBindingDescription input_bindings(BufferManager&, VertexLayout);
ArrayVertexInputs input_attributes(BufferManager&, VertexLayout, InstanceLayout);
ArrayVertexBindings input_bindings(BufferManager&, VertexLayout, InstanceLayout);