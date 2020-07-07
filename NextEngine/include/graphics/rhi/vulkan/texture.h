#pragma once

#include "volk.h"
#include "graphics/assets/texture.h"
#include "graphics/rhi/vulkan/buffer.h"

struct StagingQueue;
struct Image;
struct TextureAllocInfo;
struct TextureAllocator;
struct Assets;

//todo, TextureDesc and TextureAllocInfo, duplicate information
struct Texture {
	TextureDesc desc;
	TextureAllocInfo* alloc_info;
	VkImage image;
	VkImageView view;
};

struct Cubemap {
	VkImage image;
	TextureAllocInfo* alloc_info; //set allocation data
	VkImageView view;
};

#define MAX_IMAGE_UPLOAD mb(256)
#define MAX_IMAGE_DATA gb(1)
#define MAX_TEXTURES 200
#define MAX_MIP 14 //MAX TEXTURE SIZE is 8k

struct TextureAllocInfo {
	uint width, height;
	VkFormat format;
	VkImage image;
	TextureAllocInfo* next;
};

struct TextureAllocator {
	StagingQueue& staging_queue;
	VkDevice device;
	VkPhysicalDevice physical_device;

	HostVisibleBuffer staging;
	VkDeviceMemory image_memory;
	VkImage image;

	u64 staging_buffer_offset = 0;
	u64 image_memory_offset = 0;

	VkSampler samplers[MAX_TEXTURES] = {};

	TextureAllocInfo memory_alloc_info[MAX_TEXTURES] = {};
	uint texture_allocated_count = 0;
	TextureAllocInfo* aligned_free_list[MAX_MIP] = {}; //power of two

	TextureAllocInfo* uploaded_this_frame = NULL;	
};

const VkImageViewCreateInfo image_view_create_default = {
	VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
	nullptr,
	0,
	0,
	VK_IMAGE_VIEW_TYPE_2D,
	VK_FORMAT_R8G8B8A8_UNORM,
	{},
	{
		VK_IMAGE_ASPECT_COLOR_BIT,
		0,
		1,
		0,
		1
	}
};

const VkImageCreateInfo image_create_default = {
	VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
	nullptr,
	0,
	VK_IMAGE_TYPE_2D,
	VK_FORMAT_R8G8B8A8_UNORM,
	{0,0,1},
	1,
	1,
	VK_SAMPLE_COUNT_1_BIT,
	VK_IMAGE_TILING_OPTIMAL,
	VK_IMAGE_USAGE_SAMPLED_BIT,
	VK_SHARING_MODE_EXCLUSIVE,
	0,
	nullptr,
	VK_IMAGE_LAYOUT_UNDEFINED
};

VkImageUsageFlags to_vk_usage_flags(TextureUsage usage);

const VkImageLayout to_vk_layout[4] = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

void make_TextureAllocator(TextureAllocator&);
Texture alloc_TextureImage(TextureAllocator&, const TextureDesc&);
Texture make_TextureImage(TextureAllocator&, const Image&);
void transfer_image_ownership(TextureAllocator&, VkCommandBuffer);
void destroy_TextureAllocator(TextureAllocator&);

void blit_image(VkCommandBuffer cmd_buffer, Filter filter, struct Texture& src, ImageOffset src_region[2], Texture& dst, ImageOffset dst_region[2]);

void alloc_and_bind_memory(TextureAllocator& allocator, VkImage image);

using Sampler = VkSampler;

VkSampler get_Sampler(sampler_handle);

VkSampler make_TextureSampler(const SamplerDesc& sampler_desc);
VkImageView make_ImageView(VkDevice device, VkImage image, VkFormat imageFormat, VkImageAspectFlags aspectFlags);
void make_Image(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image);
void make_alloc_Image(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory);
//void make_TextureImage(VkImage* result, VkDeviceMemory* result_memory, StagingQueue& staging_queue, Image& image);