#pragma once

#include "volk.h"

struct StagingQueue;

struct TextureRef {
	uint mip_level;
	uint offset; //offset * power of 2 texture size, eg. 4 channel would have multiple of 4
};

TextureAllocator* make_TextureAllocator(RHI&);
void destroy_TextureAllocator(RHI&);
VkSampler make_TextureSampler(VkDevice device);
void make_Image(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
void make_TextureImage(VkImage* result, VkDeviceMemory* result_memory, StagingQueue& staging_queue, Image& image);
TextureRef make_TextureImage(TextureAllocator&, Image& image);