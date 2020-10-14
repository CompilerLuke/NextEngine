#pragma once

#include "engine/handle.h"
#include "graphics/rhi/vulkan/volk.h"
#include "graphics/rhi/vulkan/buffer.h"

struct AsyncCopyResources {
	VkDevice device;
	VkPhysicalDevice physical_device;
	HostVisibleBuffer host_visible;
	VkFence fence;
	int transfer_frame = -1;
};

void make_async_copy_resources(AsyncCopyResources& resources, uint size);
void destroy_async_copy_resources(AsyncCopyResources& resources);
void async_copy_image(VkCommandBuffer cmd_buffer, texture_handle image_handle, AsyncCopyResources& copy);
void receive_transfer(AsyncCopyResources& resources, uint size, void* dst);