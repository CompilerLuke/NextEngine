//ASYNC COPY

#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/async_cpu_copy.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/vulkan/volk.h"
#include "graphics/rhi/vulkan/buffer.h"
#include "graphics/rhi/vulkan/draw.h"

struct AsyncCopyResources {
	VkDevice device;
	VkPhysicalDevice physical_device;
	HostVisibleBuffer host_visible;
    bool buffered;
	VkFence fence;
	int transfer_frame = -1;
};

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
    //destroy_HostVisibleBuffer(rhi.device, rhi.device, resources.host_visible);
	vkDestroyFence(device, resources.fence, nullptr);
}

//it will only send a transfer once the last one has been received
void async_copy_image(CommandBuffer& cmd_buffer, texture_handle image_handle, AsyncCopyResources& copy, TextureAspect
aspect) {
	if (copy.transfer_frame != -1) return;

	Texture& texture = *get_Texture(image_handle);
	VkImage image = texture.image;

	VkBufferImageCopy region = {};
	region.imageExtent = { (uint)texture.desc.width, (uint)texture.desc.height, 1 };
	region.imageSubresource.aspectMask = to_vk_image_aspect_flags(aspect);
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.mipLevel = 0;

	vkCmdCopyImageToBuffer(cmd_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, copy.host_visible.buffer, 1, &region);

	copy.transfer_frame = rhi.frame_index;
}

void* receive_mapped_transfer(AsyncCopyResources& resources) {
    if (resources.transfer_frame == -1 || resources.transfer_frame != rhi.frame_index) return nullptr;
    resources.transfer_frame = -1;
    return resources.host_visible.mapped;
}

bool receive_transfer(AsyncCopyResources& resources, uint size, void* dst) {
    void* mapped = receive_mapped_transfer(resources);
    if(!mapped) return false;
    memcpy(dst, mapped, size);
    return true;
}
