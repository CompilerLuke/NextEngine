#include "stdafx.h"
#include <assert.h>
#include <stdio.h>
#include "core/container/array.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/device.h"

#define MAX_COMMAND_BUFFERS 20

struct CommandPool {
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue queue;
	VkCommandPool pool;
	array<MAX_COMMAND_BUFFERS, VkCommandBuffer> free;
	array<MAX_COMMAND_BUFFERS, VkCommandBuffer> submited;
};

void make_CommandPool(CommandPool& pool, VkPhysicalDevice physical_device, VkDevice device) {
	int32_t queue_family_indices = find_graphics_queue_family(physical_device);
	assert(queue_family_indices != -1);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queue_family_indices;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &pool.pool))

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = pool.pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = MAX_COMMAND_BUFFERS;

	VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, pool.free.data));
}

void destroy_CommandPool(CommandPool& pool) {
	vkDestroyCommandPool(pool.device, pool.pool, nullptr);
}

VkCommandBuffer begin_recording(CommandPool& pool) {
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkCommandBuffer cmd_buffer = pool.free.pop();

	vkBeginCommandBuffer(cmd_buffer, &beginInfo);
}

void end_recording(CommandPool& pool, VkCommandBuffer cmd_buffer) {
	VkDevice device = pool.device;

	vkEndCommandBuffer(cmd_buffer);
}

void submit(CommandPool& pool) {
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = pool.submited.length;
	submitInfo.pCommandBuffers = pool.submited.data;

	VK_CHECK(vkQueueSubmit(pool.queue, 1, &submitInfo, VK_NULL_HANDLE));
	vkQueueWaitIdle(pool.queue);

	for (VkCommandBuffer cmd_buffer : pool.submited) {
		pool.free.append(cmd_buffer);
	}

	pool.submited.clear();
}