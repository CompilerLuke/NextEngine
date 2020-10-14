#ifdef RENDER_API_VULKAN

#define STB_IMAGE_IMPLEMENTATION

#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/device.h"
#include "graphics/rhi/vulkan/buffer.h"
#include "graphics/rhi/vulkan/command_buffer.h"
#include "graphics/rhi/vulkan/draw.h"
#include <stb_image.h>
#include "graphics/assets/assets.h"
#include "core/memory/linear_allocator.h"
#include "engine/vfs.h"

//REFLECT_STRUCT_BEGIN(Texture)
//REFLECT_STRUCT_MEMBER(filename)
//REFLECT_STRUCT_END()



bool has_StencilComponent(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

struct Transition {
	VkImage image;
	VkFormat format;
	VkImageLayout old_layout;
	VkImageLayout new_layout;
};

void transition_ImageLayout(VkCommandBuffer cmd_buffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, int mips, int src_queue = VK_QUEUE_FAMILY_IGNORED, int dst_queue = VK_QUEUE_FAMILY_IGNORED) {
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = src_queue;
	barrier.dstQueueFamilyIndex = dst_queue;
	barrier.image = image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mips;
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

		//printf("TRANSITIONING 0x%p from DST_OPTIMAL TO TRANSFER_DST_OPTIMAL, from %i, to %i\n", image, src_queue, dst_queue);
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		//this could cause a bug, if the vertex shader reads images as well, eg. terrain rendering

		//printf("TRANSITIONING 0x%p from DST_OPTIMAL TO SHADER READ OPTIMAL, from %i, to %i\n", image, src_queue, dst_queue);
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

		//printf("TRANSITIONING 0x%p from DST_OPTIMAL TO SHADER READ OPTIMAL, from %i, to %i\n", image, src_queue, dst_queue);
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} 
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		printf("TRANSITIONING 0x%p from UNDEFINED TO DEPTH STENCIL OPTIMAL\n", image);

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

	//makeInfo.flags = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

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

VkFilter to_vk_filter[] = { VK_FILTER_NEAREST, VK_FILTER_LINEAR };
VkSamplerMipmapMode to_vk_mipmap_mode[] = { VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR };
VkSamplerAddressMode to_vk_address[] = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_REPEAT };


VkSampler make_TextureSampler(const SamplerDesc& sampler_desc) {
	VkDevice device = rhi.device;

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter =  to_vk_filter[(uint)sampler_desc.mag_filter];
	samplerInfo.minFilter = to_vk_filter[(uint)sampler_desc.min_filter];
	samplerInfo.addressModeU = to_vk_address[(uint)sampler_desc.wrap_u];
	samplerInfo.addressModeV = to_vk_address[(uint)sampler_desc.wrap_v];
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = sampler_desc.max_anisotropy > 0;
	samplerInfo.maxAnisotropy = sampler_desc.max_anisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = to_vk_mipmap_mode[(uint)sampler_desc.mip_mode];
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = FLT_MAX;


	VkSampler texture_sampler;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &texture_sampler) != VK_SUCCESS) {
		throw "Failed to make texture sampler!";
	}

	return texture_sampler;
}

//TEXTURE ALLOCATOR


//Texture Allocator, will upload all textures into one massive texture
//This has has several pros in terms of performance, as it allows for batching of uploads and memory transfers
//However, the current system will waste a significant amount of memory if not fully utilized
//One limitation is that the image is of a single format 4-channels, all textures loaded so far have been in this format
//However, this requires some form of texture packing eg. albedo in rgb, roughness in a, normal in rgb, roughness in a
//Overall this seems like a plausible strategy, as it may allow for multi draw indirect and virtual texturing

void make_TextureAllocator(TextureAllocator& allocator) {
	//todo there must be a better way of getting this resource

	VkDevice device = rhi.device;
	VkPhysicalDevice physical_device = rhi.device;

	allocator.device = device;
	allocator.physical_device = physical_device;

	int image_size = sqrt(MAX_IMAGE_DATA / 4 / 2);

	printf("TOTAL AVAILABLE IMAGE TEXTURE: %ix%i\n", image_size, image_size);

	allocator.staging = make_HostVisibleBuffer(device, physical_device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MAX_IMAGE_UPLOAD);
	
	//make_Buffer(device, physical_device, MAX_IMAGE_UPLOAD, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocator.staging_buffer, allocator.staging_buffer_memory);
	
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

	if (vkAllocateMemory(device, &allocInfo, nullptr, &allocator.image_memory) != VK_SUCCESS) {
		throw "Could not allocate memory for image!";
	}

	map_buffer_memory(device, allocator.staging);
	
	//make_Image(device, physical_device, image_size, image_size, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator->image, allocator->image_memory);
}

int select_mip(int width, int height) {
	return (int)glm::ceil(glm::log2((double)glm::max(width, height)));
}

VkImageUsageFlags to_vk_usage_flags(TextureUsage usage) {
	VkImageUsageFlags usage_flags = {};

	if (usage & TextureUsage::Sampled) usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (usage & TextureUsage::TransferSrc) usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (usage & TextureUsage::TransferDst) usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (usage & TextureUsage::ColorAttachment) usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (usage & TextureUsage::InputAttachment) usage_flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	
	return usage_flags;
}

VkFormat to_vk_image_format(TextureFormat format, uint num_channels) {
	VkFormat formats_by_channel_count[4][4] = {
		{ VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM },
		{VK_FORMAT_R8_SRGB, VK_FORMAT_R8G8_SRGB, VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_R8G8B8A8_SRGB},
		{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT},
		{VK_FORMAT_R8_UINT, VK_FORMAT_R8G8_UINT, VK_FORMAT_R8G8B8_UINT, VK_FORMAT_R8G8B8A8_UINT}
	};

	return formats_by_channel_count[(uint)format][num_channels - 1];
}

VkFormat to_vk_image_format(const TextureDesc& desc) {
	return to_vk_image_format(desc.format, desc.num_channels);
}

Texture alloc_TextureImage(TextureAllocator& allocator, const TextureDesc& desc) {
	uint width = desc.width;
	uint height = desc.height;
	uint num_channels = desc.num_channels;
	
	VkDevice device = allocator.device;
	VkPhysicalDevice physical_device = allocator.physical_device;
	StagingQueue& staging_queue = allocator.staging_queue;

	VkFormat image_format = to_vk_image_format(desc);

	u64 texel_sizes[3] = { 1, 1, 4 };
	u64 texel_alignment = texel_sizes[(uint)desc.format] * desc.num_channels;
	VkDeviceSize image_size = desc.width * desc.height * desc.num_channels * texel_sizes[(uint)desc.format];

	/*
	int32_t offset = allocator.staging_buffer_offset;
	memcpy_Buffer(device, allocator.staging_buffer_memory, pixels, image_size, offset);
	allocator.staging_buffer_offset += image_size;
	*/

	int mip = select_mip(width, height);
	assert(mip < MAX_MIP);
	TextureAllocInfo* info = allocator.aligned_free_list[(int)mip];

	while (info != NULL) {
		if (info->width <= width && info->height <= height && info->format == image_format) {
			break;
		}
		info = info->next;
	}

	VkImage vk_image;

	if (info == NULL) {
		//todo parameterize transfer flags!
		
		VkImageUsageFlags usage_flags = to_vk_usage_flags(desc.usage);

		make_Image(device, physical_device, width, height, image_format, VK_IMAGE_TILING_OPTIMAL, usage_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vk_image);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, vk_image, &memRequirements);

		allocator.image_memory_offset += allocator.image_memory_offset % memRequirements.alignment;
		vkBindImageMemory(device, vk_image, allocator.image_memory, allocator.image_memory_offset);

		allocator.image_memory_offset += memRequirements.size;

		info = &allocator.memory_alloc_info[allocator.texture_allocated_count++];
		info->width = width;
		info->height = height;
		info->format = image_format;
		info->image = vk_image;
	}
	else {
		vk_image = info->image;
		assert(false);
	}

	Texture result;
	result.desc = desc;
	result.alloc_info = info;
	result.image = vk_image;
	result.view = make_ImageView(device, vk_image, image_format, VK_IMAGE_ASPECT_COLOR_BIT);

	assert(info->image != NULL);

	return result;
}

void blit_image(VkCommandBuffer cmd_buffer, Filter filter, Texture& src, ImageOffset src_region[2], Texture& dst, ImageOffset dst_region[2]) {

	VkImageBlit region = {};
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.baseArrayLayer = 0;
	region.srcSubresource.layerCount = 1;
	region.srcSubresource.mipLevel = 0;
	region.srcOffsets[0].x = src_region[0].x;
	region.srcOffsets[0].y = src_region[0].y;
	region.srcOffsets[1].x = src_region[1].x;
	region.srcOffsets[1].y = src_region[1].y;
	region.srcOffsets[1].z = 1;

	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.baseArrayLayer = 0;
	region.dstSubresource.layerCount = 1;
	region.dstSubresource.mipLevel = 0;
	region.dstOffsets[0].x = dst_region[0].x;
	region.dstOffsets[0].y = dst_region[0].y;
	region.dstOffsets[1].x = dst_region[1].x;
	region.dstOffsets[1].y = dst_region[1].y;
	region.dstOffsets[1].z = 1;

	vkCmdBlitImage(cmd_buffer, src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, to_vk_filter[(uint)filter]);
}


void alloc_and_bind_memory(TextureAllocator& allocator, VkImage image) {
	VkDevice device = allocator.device;
	
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(allocator.device, image, &memRequirements);

	u64 offset = aligned_incr(&allocator.image_memory_offset, memRequirements.size, memRequirements.alignment);
	assert(allocator.image_memory_offset < MAX_IMAGE_DATA);

	vkBindImageMemory(device, image, allocator.image_memory, offset);
}

Texture make_TextureImage(TextureAllocator& allocator, const Image& image) {
	VkDevice device = allocator.device;
	VkPhysicalDevice physical_device = allocator.physical_device;
	StagingQueue& staging_queue = allocator.staging_queue;

	assert(staging_queue.recording);

	//texture_desc.internal_format = InternalColorFormat::R32I;
	//texture_desc.external_format = ColorFormat::Rgba;
	//texture_desc.texel_type = TexelType::Int;


	//(uint)desc.internal_format

	VkFormat image_format = to_vk_image_format(image);

	void* pixels = image.data;

	u64 texel_sizes[4] = { 1, 1, 4, 1 };
	u64 texel_alignment = texel_sizes[(uint)image.format] * image.num_channels;
	VkDeviceSize image_size = image.width * image.height * image.num_channels * texel_sizes[(uint)image.format];

	if (!pixels) throw "Failed to load texture image!";


	int32_t offset = aligned_incr(&allocator.staging_buffer_offset, image_size, texel_alignment);
	memcpy((char*)allocator.staging.mapped + offset, pixels, image_size);

	assert(allocator.staging_buffer_offset < MAX_IMAGE_UPLOAD);
	assert(offset % texel_alignment == 0);

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

		alloc_and_bind_memory(allocator, vk_image);

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

	VkCommandBuffer cmd_buffer = staging_queue.cmd_buffers[staging_queue.frame_index]; 

	printf("COPYING DATA TO IMAGE : 0x%p %ix%i\n", vk_image, info->width, info->height);

	transition_ImageLayout(cmd_buffer, vk_image, image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
	copy_buffer_to_image(cmd_buffer, allocator.staging.buffer, vk_image, image.width, image.height, offset);
	transition_ImageLayout(cmd_buffer, vk_image, image_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, staging_queue.queue_family, staging_queue.dst_queue_family);
	
	Texture result;
	result.desc = image;
	result.alloc_info = info;
	result.image = vk_image;
	result.view = make_ImageView(device, vk_image, image_format, VK_IMAGE_ASPECT_COLOR_BIT);

	assert(info->image != NULL);

	return result;
}

void transfer_image_ownership(TextureAllocator& allocator, VkCommandBuffer cmd_buffer) {
	StagingQueue& queue = allocator.staging_queue;
	TextureAllocInfo* transfer_ownership = allocator.uploaded_this_frame;

	while (transfer_ownership) {
		printf("===========================\n");
		printf("TRANSFERRING OWNERSHIP OF IMAGE : 0x%p %ix%i\n", transfer_ownership->image, transfer_ownership->width, transfer_ownership->height);

		transition_ImageLayout(cmd_buffer, transfer_ownership->image, transfer_ownership->format, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, queue.queue_family, queue.dst_queue_family);
		//transition_ImageLayout(cmd_buffer, allocator.staging_queue, transfer_ownership->image, transfer_ownership->format,
		//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		printf("\n=========================\n");

		transfer_ownership = transfer_ownership->next;
	}

	allocator.uploaded_this_frame = NULL;
}

//todo could track layout, would remove the need for TextureLayout from, at the cost of 
//1. potential threading problems
//2. being less explicit and clear
void transition_layout(CommandBuffer& cmd_buffer, texture_handle handle, TextureLayout from, TextureLayout to) {
	Texture* texture = get_Texture(handle);
	VkFormat format = to_vk_image_format(texture->desc);

	transition_ImageLayout(cmd_buffer, texture->image, format, to_vk_layout[(uint)from], to_vk_layout[(uint)to], texture->desc.num_mips);
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

	unmap_buffer_memory(device, allocator->staging);
	
	for (int i = 0; i < MAX_TEXTURES; i++) {
		TextureAllocInfo info = allocator->memory_alloc_info[i];
		if (info.format == 0) continue;
	
		vkDestroyImage(device, info.image, nullptr);
	}
	vkFreeMemory(device, allocator->image_memory, nullptr);

	vkDestroyBuffer(allocator->device, allocator->staging.buffer, NULL);
	vkFreeMemory(allocator->device, allocator->staging.buffer_memory, NULL);
}

#endif