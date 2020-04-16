#pragma once

#include "volk.h"
#include "graphics/assets/texture.h"

struct StagingQueue;
struct Image;
struct TextureAllocInfo;
struct TextureAllocator;
struct Assets;

struct Texture {
	TextureDesc desc;
	TextureAllocInfo* alloc_info;
	VkImage image;
	VkImageView view;
};

TextureAllocator* make_TextureAllocator(RHI&);
void destroy_TextureAllocator(RHI&);
VkSampler make_TextureSampler(VkDevice device);
VkImageView make_ImageView(VkDevice device, VkImage image, VkFormat imageFormat, VkImageAspectFlags aspectFlags);
void make_Image(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image);
void make_alloc_Image(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory);
void make_TextureImage(VkImage* result, VkDeviceMemory* result_memory, StagingQueue& staging_queue, Image& image);
Texture make_TextureImage(TextureAllocator&, TextureDesc& desc, Image image);