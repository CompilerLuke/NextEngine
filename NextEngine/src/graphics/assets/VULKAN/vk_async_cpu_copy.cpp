//ASYNC COPY

#include "graphics/rhi/vulkan/async_cpu_copy.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/assets/assets.h"

AsyncCopyResources* make_async_copy_resources(uint size) {
    AsyncCopyResources& resources = *PERMANENT_ALLOC(AsyncCopyResources);
	VkDevice device = rhi.device;
	VkPhysicalDevice physical_device = rhi.device;

	resources.host_visible = make_HostVisibleBuffer(device, physical_device, VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);
	map_buffer_memory(device, resources.host_visible);
    
    return &resources;
}

void destroy_async_copy_resources(AsyncCopyResources& resources) {
	VkDevice device = rhi.device;
	unmap_buffer_memory(device, resources.host_visible);
	vkDestroyFence(device, resources.fence, nullptr);
}

//this is somewhat hacky but it will only send a transfer once the last one has been received
void async_copy_image(VkCommandBuffer cmd_buffer, texture_handle image_handle, AsyncCopyResources& copy) {
	if (copy.transfer_frame != -1) return;

	Texture& texture = *get_Texture(image_handle);
	VkImage image = texture.image;

	VkBufferImageCopy region = {};
	region.imageExtent = { (uint)texture.desc.width, (uint)texture.desc.height, 1 };
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.mipLevel = 0;

	vkCmdCopyImageToBuffer(cmd_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, copy.host_visible.buffer, 1, &region);

	copy.transfer_frame = rhi.frame_index;
}

void receive_transfer(AsyncCopyResources& resources, uint size, void* dst) {
	if (resources.transfer_frame == -1 || resources.transfer_frame != rhi.frame_index) return;
	memcpy(dst, resources.host_visible.mapped, size);
	resources.transfer_frame = -1;
}
