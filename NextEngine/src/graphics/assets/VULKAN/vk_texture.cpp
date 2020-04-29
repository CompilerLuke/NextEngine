#include "stdafx.h"
#ifdef RENDER_API_VULKAN

#define STB_IMAGE_IMPLEMENTATION

#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/device.h"
#include "graphics/rhi/vulkan/buffer.h"
#include "graphics/rhi/vulkan/command_buffer.h"
#include <stb_image.h>
#include "graphics/assets/assets.h"
#include "graphics/rhi/vulkan/texture.h"
#include "core/memory/linear_allocator.h"
#include "core/io/vfs.h"

//REFLECT_STRUCT_BEGIN(Texture)
//REFLECT_STRUCT_MEMBER(filename)
//REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Cubemap)
REFLECT_STRUCT_MEMBER(filename)
REFLECT_STRUCT_END()

struct Assets;

Image load_Image(Assets& assets, string_view filename) {
	string_buffer real_filename = tasset_path(assets, filename); //level.asset_path(filename);

	if (filename.starts_with("Aset") || filename.starts_with("tgh") || filename.starts_with("ta") || filename.starts_with("smen")) stbi_set_flip_vertically_on_load(true);
	else stbi_set_flip_vertically_on_load(false);

	Image image;

	image.data = stbi_load(real_filename.c_str(), &image.width, &image.height, &image.num_channels, STBI_rgb_alpha); //todo might waste space
	image.num_channels = 4;
	if (!image.data) throw "Could not load texture";

	return image;
}

void free_Image(Image& image) {
	stbi_image_free(image.data);
}

//void ENGINE_API gl_bind_cubemap(CubemapManager&, cubemap_handle, uint);
//void ENGINE_API gl_bind_to(TextureManager&, texture_handle, uint);
//int  ENGINE_API gl_id_of(TextureManager&, texture_handle);
int  ENGINE_API gl_format(InternalColorFormat format);
int  ENGINE_API gl_format(ColorFormat format);
int  ENGINE_API gl_format(TexelType format);
int  ENGINE_API gl_format(Filter filter);
int  ENGINE_API gl_format(Wrap wrap);
int  ENGINE_API gl_gen_texture();
void ENGINE_API gl_copy_sub(int width, int height); //todo abstract away
uint ENGINE_API gl_submit(Image&);

void gl_copy_sub(int width, int height) {
	/**/
}

//todo this should really be an array lookup
int gl_format(InternalColorFormat format) {
	if (format == InternalColorFormat::Rgb16f) return VK_FORMAT_R16G16B16A16_SFLOAT;
	if (format == InternalColorFormat::R32I) return VK_FORMAT_R32_SINT;
	if (format == InternalColorFormat::Red) return VK_FORMAT_R16_SFLOAT;
	return 0;
}

int gl_format(ColorFormat format) {
	if (format == ColorFormat::Rgb) return VK_FORMAT_R16G16B16A16_SFLOAT;
	if (format == ColorFormat::Red_Int) return VK_FORMAT_R32_SINT;
	if (format == ColorFormat::Red) return VK_FORMAT_R16_SFLOAT;
	return 0;
}

int gl_format(TexelType format) {
	if (format == TexelType::Float) return VK_FORMAT_R16_SFLOAT;
	if (format == TexelType::Int) return VK_FORMAT_R16_SINT;
	return 0;
}

int gl_format(Filter filter) {
	if (filter == Filter::Nearest) return VK_FILTER_NEAREST;
	if (filter == Filter::Linear) return VK_FILTER_LINEAR;
	if (filter == Filter::LinearMipmapLinear) return VK_FILTER_LINEAR;
	return 0;
}

int gl_format(Wrap wrap) {
	if (wrap == Wrap::ClampToBorder) return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	if (wrap == Wrap::Repeat) return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	return 0;
}

int gl_gen_texture() {
	unsigned int tex = 0;
	/**/

	return tex;
}

bool has_StencilComponent(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

struct Transition {
	VkImage image;
	VkFormat format;
	VkImageLayout old_layout;
	VkImageLayout new_layout;
};

void transition_ImageLayout(VkCommandBuffer cmd_buffer, StagingQueue& queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, bool transfer = false) {
	int src_queue = transfer ? queue.queue_family : VK_QUEUE_FAMILY_IGNORED;
	int dst_queue = transfer ? queue.dst_queue_family : VK_QUEUE_FAMILY_IGNORED;

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = src_queue;
	barrier.dstQueueFamilyIndex = dst_queue;
	barrier.image = image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (has_StencilComponent(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		//this could cause a bug, if the vertex shader reads images as well, eg. terrain rendering
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		
		if (src_queue == dst_queue) {
			fprintf(stderr, "If layout is the same transition expects to release ownership, however src and dst queue families were the same");
		}
	}
	else {
		throw "Unsupported layout transition";
	}

	vkCmdPipelineBarrier(
		cmd_buffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	//endSingleTimeCommands(queue, cmd_buffer);
}

void copy_buffer_to_image(VkCommandBuffer cmd_buffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t offset = 0) {
	VkBufferImageCopy region = {};
	region.bufferOffset = offset;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0,0,0 };
	region.imageExtent = {
		width, height, 1
	};

	vkCmdCopyBufferToImage(cmd_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

VkImageView make_ImageView(VkDevice device, VkImage image, VkFormat imageFormat, VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo makeInfo = {}; //todo abstract image view creation
	makeInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	makeInfo.image = image;
	makeInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	makeInfo.format = imageFormat;

	makeInfo.subresourceRange.aspectMask = aspectFlags;
	makeInfo.subresourceRange.baseMipLevel = 0;
	makeInfo.subresourceRange.levelCount = 1;
	makeInfo.subresourceRange.baseArrayLayer = 0;
	makeInfo.subresourceRange.layerCount = 1;

	VkImageView image_view;
	if (vkCreateImageView(device, &makeInfo, nullptr, &image_view) != VK_SUCCESS) {
		throw "Failed to make image views!";
	}

	return image_view;
}

//reminder: image memory must be bound afterwards!
void make_Image(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image) {
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage; 
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	if (vkCreateImage(device, &imageInfo, nullptr, image) != VK_SUCCESS) {
		throw "Could not make image!";
	}
}

void make_alloc_Image(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory) {
	make_Image(device, physical_device, width, height, format, tiling, usage, properties, image);

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, *image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type(physical_device, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, imageMemory) != VK_SUCCESS) {
		throw "Could not allocate memory for image!";
	}

	vkBindImageMemory(device, *image, *imageMemory, 0);
}

//todo this function is never used, delete it
/*void make_TextureImage(VkImage* result, VkDeviceMemory* result_memory,  StagingQueue& staging_queue, Image& image, int32_t transfer_dst) {
	VkDevice device = staging_queue.device;
	VkPhysicalDevice physical_device = staging_queue.physical_device;

	void* pixels = image.data;

	VkDeviceSize imageSize = image.width * image.height * 4;

	if (!pixels) throw "Failed to load texture image!";

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	make_Buffer(device, physical_device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, imageSize);
	vkUnmapMemory(device, stagingBufferMemory);

	make_alloc_Image(device, physical_device, image.width, image.height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, result, result_memory);

	transition_ImageLayout(staging_queue, *result, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copy_buffer_to_image(staging_queue, stagingBuffer, *result, image.width, image.height);
	transition_ImageLayout(staging_queue, *result, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, transfer_dst);

	staging_queue.destroyBuffers.append(stagingBuffer);
	staging_queue.destroyDeviceMemory.append(stagingBufferMemory);
}*/


VkSampler make_TextureSampler(VkDevice device) {
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	VkSampler texture_sampler;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &texture_sampler) != VK_SUCCESS) {
		throw "Failed to make texture sampler!";
	}

	return texture_sampler;
}


//TEXTURE ALLOCATOR

struct TextureAllocInfo {
	uint width, height;
	VkFormat format;
	VkImage image;
	TextureAllocInfo* next;
};

#define MAX_IMAGE_UPLOAD mb(256)
#define MAX_IMAGE_DATA gb(1)
#define MAX_TEXTURES 200
#define MAX_MIP 14 //MAX TEXTURE SIZE is 8k

struct TextureAllocator {	
	StagingQueue& staging_queue;
	VkDevice device;
	VkPhysicalDevice physical_device;

	VkDeviceMemory staging_buffer_memory;
	VkDeviceMemory image_memory;
	VkBuffer staging_buffer;
	VkImage image;

	u64 staging_buffer_offset = 0;
	u64 image_memory_offset = 0;

	TextureAllocInfo memory_alloc_info[MAX_TEXTURES] = {};
	uint texture_allocated_count = 0;
	TextureAllocInfo* aligned_free_list[MAX_MIP] = {}; //power of two

	TextureAllocInfo* uploaded_this_frame = NULL;
};

//Texture Allocator, will upload all textures into one massive texture
//This has has several pros in terms of performance, as it allows for batching of uploads and memory transfers
//However, the current system will waste a significant amount of memory if not fully utilized
//One limitation is that the image is of a single format 4-channels, all textures loaded so far have been in this format
//However, this requires some form of texture packing eg. albedo in rgb, roughness in a, normal in rgb, roughness in a
//Overall this seems like a plausible strategy, as it may allow for multi draw indirect and virtual texturing

TextureAllocator* make_TextureAllocator(RHI& rhi, StagingQueue& queue) {
	//todo there must be a better way of getting this resource
	TextureAllocator* allocator = PERMANENT_ALLOC(TextureAllocator, {queue});

	VkDevice device = get_Device(rhi);
	VkPhysicalDevice physical_device = get_PhysicalDevice(rhi);

	allocator->device = device;
	allocator->physical_device = physical_device;

	int image_size = sqrt(MAX_IMAGE_DATA / 4 / 2);

	printf("TOTAL AVAILABLE IMAGE TEXTURE: %ix%i\n", image_size, image_size);

	make_Buffer(device, physical_device, MAX_IMAGE_UPLOAD, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator->staging_buffer, allocator->staging_buffer_memory);
	
	VkImage image;
	make_Image(device, physical_device, 256, 256, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image);

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = MAX_IMAGE_DATA;
	allocInfo.memoryTypeIndex = find_memory_type(physical_device, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//MEMORY TYPE BITS 130

	printf("MEMORY TYPE BITS %i\n", memRequirements.memoryTypeBits);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &allocator->image_memory) != VK_SUCCESS) {
		throw "Could not allocate memory for image!";
	}
	
	//make_Image(device, physical_device, image_size, image_size, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator->image, allocator->image_memory);
	return allocator;
}

int select_mip(int width, int height) {
	return (int)glm::ceil(glm::log2((double)glm::max(width, height)));
}

Texture make_TextureImage(TextureAllocator& allocator, TextureDesc& desc, Image image) {
	VkDevice device = allocator.device;
	VkPhysicalDevice physical_device = allocator.physical_device;
	StagingQueue& staging_queue = allocator.staging_queue;

	VkFormat formats_by_channel_count[] = {VK_FORMAT_R8_SRGB, VK_FORMAT_R8G8_SRGB, VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_R8G8B8A8_SRGB};
	VkFormat image_format = formats_by_channel_count[image.num_channels - 1];

	void* pixels = image.data;

	VkDeviceSize image_size = image.width * image.height * image.num_channels;

	if (!pixels) throw "Failed to load texture image!";

	int32_t offset = allocator.staging_buffer_offset;
	memcpy_Buffer(device, allocator.staging_buffer_memory, pixels, image_size, offset);
	allocator.staging_buffer_offset += image_size;

	int mip = select_mip(image.width, image.height);
	assert(mip < MAX_MIP);
	TextureAllocInfo* info = allocator.aligned_free_list[(int)mip];

	while (info != NULL) {
		if (info->width <= image.width && info->height <= image.height && info->format == image_format) {
			break;
		}
		info = info->next;
	}

	VkImage vk_image;

	if (info == NULL) {
		make_Image(device, physical_device, image.width, image.height, image_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vk_image);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, vk_image, &memRequirements);

		allocator.image_memory_offset += allocator.image_memory_offset % memRequirements.alignment;
		vkBindImageMemory(device, vk_image, allocator.image_memory, allocator.image_memory_offset);

		allocator.image_memory_offset += memRequirements.size;

		info = &allocator.memory_alloc_info[allocator.texture_allocated_count++];
		info->width = image.width;
		info->height = image.height;
		info->format = image_format;
		info->image = vk_image;
	}
	else {
		vk_image = info->image;
		assert(false);
	}

	//THIS WAY WE CAN EXECUTE ALL THE RESOURCE TRANSFERS IN THE GRAPHICS QUEUE
	TextureAllocInfo* next_info = allocator.uploaded_this_frame;
	allocator.uploaded_this_frame = info;
	info->next = next_info;

	VkCommandBuffer cmd_buffer = staging_queue.cmd_buffers[staging_queue.frame_index]; // beginSingleTimeCommands(queue);

	transition_ImageLayout(cmd_buffer, staging_queue, vk_image, image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copy_buffer_to_image(cmd_buffer, allocator.staging_buffer, vk_image, image.width, image.height, offset);
	transition_ImageLayout(cmd_buffer, staging_queue, vk_image, image_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);

	Texture result;
	result.desc = desc;
	result.alloc_info = info;
	result.image = vk_image;
	result.view = make_ImageView(device, vk_image, image_format, VK_IMAGE_ASPECT_COLOR_BIT);

	assert(info->image != NULL);

	return result;
}

void transfer_image_ownership(TextureAllocator& allocator, VkCommandBuffer cmd_buffer) {
	TextureAllocInfo* transfer_ownership = allocator.uploaded_this_frame;

	while (transfer_ownership) {
		transition_ImageLayout(cmd_buffer, allocator.staging_queue, transfer_ownership->image, transfer_ownership->format, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
		//transition_ImageLayout(cmd_buffer, allocator.staging_queue, transfer_ownership->image, transfer_ownership->format,
		//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		transfer_ownership = transfer_ownership->next;
	}


	allocator.uploaded_this_frame = NULL;
}

void destroy_TextureImage(TextureAllocator& allocator, Texture& texture) {
	TextureAllocInfo* info = texture.alloc_info;

	int mip = select_mip(info->width, info->height);
	
	TextureAllocInfo* next = allocator.aligned_free_list[mip];
	allocator.aligned_free_list[mip] = info;
	info->next = next;
}

void destroy_TextureAllocator(TextureAllocator* allocator) {
	VkDevice device = allocator->device;
	VkPhysicalDevice physical_device = allocator->physical_device;
	
	for (int i = 0; i < MAX_TEXTURES; i++) {
		TextureAllocInfo info = allocator->memory_alloc_info[i];
		if (info.format == 0) continue;
	
		vkDestroyImage(device, info.image, nullptr);
	}
	vkFreeMemory(device, allocator->image_memory, nullptr);

	vkDestroyBuffer(allocator->device, allocator->staging_buffer, NULL);
	vkFreeMemory(allocator->device, allocator->staging_buffer_memory, NULL);
}

#endif