#include "stdafx.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include <stdio.h>
#include "core/container/vector.h"
#include "core/io/vfs.h"
#include "core/memory/linear_allocator.h"


#include "stb_image.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"
#include "graphics/assets/shader.h"
#include "graphics/assets/material.h"

#include "graphics/rhi/rhi.h"
#include "graphics/rhi/primitives.h"
#include "graphics/rhi/window.h"

#include "graphics/rhi/vulkan/rhi.h"
#include "graphics/rhi/vulkan/buffer.h"
#include "graphics/rhi/vulkan/pipeline.h"
#include "graphics/rhi/vulkan/shader.h"
#include "graphics/rhi/vulkan/device.h"
#include "graphics/rhi/vulkan/swapchain.h"
#include "graphics/rhi/vulkan/texture.h"

struct RHI {
	Window& window;
	VulkanDesc desc;
	Device device;
	Swapchain swapchain;
	BufferAllocator* buffer_manager;
	VkQueue graphics_queue;
	VkQueue present_queue;
	VkCommandPool cmd_pool;
};

VkDescriptorSetLayout descriptorSetLayout;
VkDescriptorPool descriptorPool;

array<MAX_SWAP_CHAIN_IMAGES, VkDescriptorSet> descriptorSets;

VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;

VkRenderPass renderPass;

array<MAX_SWAP_CHAIN_IMAGES, VkCommandBuffer> commandBuffers;


/*
struct BufferAllocator {
VkDeviceMemory block;

};

struct Buffer {
VkBuffer buffer;
};


BufferAllocator buffer_manager;
*/

//Resources
VertexBuffer vertex_buffer;
InstanceBuffer instance_buffer;

array<MAX_SWAP_CHAIN_IMAGES, VkBuffer> uniform_buffers;
array<MAX_SWAP_CHAIN_IMAGES, VkDeviceMemory> uniform_buffers_memory;

VkImage texture_image;
VkDeviceMemory texture_image_memory;
VkImageView texture_image_view;
VkSampler texture_sampler;

VkImage depth_image;
VkDeviceMemory depth_image_memory;
VkImageView depth_image_view;

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

	VkImageView imageView;
	if (vkCreateImageView(device, &makeInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw "Failed to make image views!";
	}

	return imageView;
}

void make_ImageViews(VkDevice device, Swapchain& swapchain) {
	swapchain.image_views.resize(swapchain.images.length);

	for (int i = 0; i < swapchain.images.length; i++) {
		swapchain.image_views[i] = make_ImageView(device, swapchain.images[i], swapchain.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void make_TextureImageView(VkDevice device) {
	texture_image_view = make_ImageView(device, texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}

struct UniformBufferObject {
	glm::mat4 view;
	glm::mat4 proj;
};

struct CmdCopyBuffer {
	VkBuffer srcBuffer;
	VkBuffer dstBuffer;
	VkBufferCopy copyRegion;
};

struct CmdCopyBufferToImage {
	VkBuffer buffer;
	VkImage image;
	VkImageLayout imageLayout;
	VkBufferImageCopy copyRegion;
};

struct CmdPipelineBarrier {
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags dstStage;
	VkImageMemoryBarrier imageBarrier;
};

#include <glm/gtc/matrix_transform.hpp>
#include "core/container/tvector.h"

tvector<glm::mat4> instances;

void upload_MeshData(Assets& assets, BufferAllocator& buffer_manager) {
	model_handle handle = load_Model(assets, "Aset_wood_log_L_rdeu3_LOD0.fbx");
	Model* model = get_Model(assets, handle);
	
	instances.reserve(10 * 10);
	for (int x = 0; x < 10; x++) {
		for (int y = 0; y < 10; y++) {
			glm::mat4 model(1.0);
			model = glm::translate(model, glm::vec3(x * 50, y * 20, 0));
			model = glm::scale(model, glm::vec3(0.2f));
			instances.append(model);
		}
	}

	vertex_buffer = model->meshes[0].buffer;
	instance_buffer = alloc_instance_buffer<glm::mat4>(buffer_manager, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4, instances);

}

void make_UniformBuffers(VkDevice device, VkPhysicalDevice physical_device, int frames_in_flight) {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	uniform_buffers.resize(frames_in_flight);
	uniform_buffers_memory.resize(frames_in_flight);

	for (int i = 0; i < frames_in_flight; i++) {
		make_Buffer(device, physical_device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniform_buffers[i], uniform_buffers_memory[i]);
	}
}

void make_DescriptorPool(VkDevice device, int frames_in_flight) {
	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = frames_in_flight;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = frames_in_flight;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = frames_in_flight;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw "Failed to make descriptor pool!";

	}
}


string_buffer read_file_or_fail(Assets& assets, string_view src) {
	string_buffer output;
	if (!readf(assets, src, &output)) throw "Failed to load vertex shaders!";
	return output;
}

ArrayVertexInputs Vertex_attribute_descriptions(BufferAllocator& buffer_manager) {
	return input_attributes(buffer_manager, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
}

ArrayVertexBindings Vertex_binding_descriptors(BufferAllocator& buffer_manager) {
	return input_bindings(buffer_manager, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
}

void make_GraphicsPipeline(VkDevice device, VkRenderPass render_pass, VkExtent2D extent, BufferAllocator& buffer_manager, VkShaderModule vert_shader, VkShaderModule frag_shader) {
	size_t before = temporary_allocator.occupied;

	//NEEDS REFERENCE
	auto binding_descriptors = Vertex_binding_descriptors(buffer_manager);

	PipelineDesc pipeline_desc;
	pipeline_desc.vert_shader = vert_shader;
	pipeline_desc.frag_shader = frag_shader;
	pipeline_desc.extent = extent;
	pipeline_desc.attribute_descriptions = Vertex_attribute_descriptions(buffer_manager);
	pipeline_desc.binding_descriptions = binding_descriptors;
	pipeline_desc.descriptor_layouts = descriptorSetLayout;
	pipeline_desc.render_pass = renderPass;

	make_GraphicsPipeline(device, pipeline_desc, &pipelineLayout, &graphicsPipeline);

	vkDestroyShaderModule(device, pipeline_desc.vert_shader, nullptr);
	vkDestroyShaderModule(device, pipeline_desc.frag_shader, nullptr);

	temporary_allocator.reset(before);
}


VkFormat find_depth_format(VkPhysicalDevice);

void make_RenderPass(VkDevice device, VkPhysicalDevice physical_device, VkFormat image_format) {
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = image_format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = find_depth_format(physical_device);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw "Failed to make render pass!";
	}

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
}

void make_Framebuffers(VkDevice device, Swapchain& swapchain) {
	swapchain.framebuffers.resize(swapchain.images.length);

	for (int i = 0; i < swapchain.images.length; i++) {
		VkImageView attachments[2] = {
			swapchain.image_views[i],
			depth_image_view
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swapchain.extent.width;
		framebufferInfo.height = swapchain.extent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchain.framebuffers[i]) != VK_SUCCESS) {
			throw "failed to make framebuffer!";
		}
	}
}

VkCommandPool make_CommandPool(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	int32_t graphics_family = find_graphics_queue_family(physical_device);
	assert(graphics_family != -1);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = graphics_family;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool result;

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &result) != VK_SUCCESS) {
		throw "Could not make pool!";
	}

	return result;
}

int frames_in_flight(Swapchain& swapchain) {
	return swapchain.images.length;
}

void record_CommandBuffers(VkDevice device, BufferAllocator& buffer_allocator, Swapchain& swapchain, int32_t frame_index) {
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = 0;
	begin_info.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(commandBuffers[frame_index], &begin_info) != VK_SUCCESS) {
		throw "Failed to begin recording command buffer!";
	}

	VkRenderPassBeginInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = renderPass;
	render_pass_info.framebuffer = swapchain.framebuffers[frame_index];
	render_pass_info.renderArea.offset = { 0,0 };
	render_pass_info.renderArea.extent = swapchain.extent;

	VkClearValue clear_colors[2];
	clear_colors[0].color = { 0.1f, 0.1f, 0.1f, 1.0f };
	clear_colors[1].depthStencil = { 1, 0 };

	render_pass_info.clearValueCount = 2;
	render_pass_info.pClearValues = clear_colors;



	vkCmdBeginRenderPass(commandBuffers[frame_index], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffers[frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	/*
	vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertex_buffers, offsets);
	vkCmdBindVertexBuffers(commandBuffers[i], 1, 1, instance_buffers, offsets);
	vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
	*/

	bind_vertex_buffer(buffer_allocator, commandBuffers[frame_index], VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
	vkCmdBindDescriptorSets(commandBuffers[frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[frame_index], 0, nullptr);


	vkCmdDrawIndexed(commandBuffers[frame_index], vertex_buffer.length, instance_buffer.length, vertex_buffer.index_base, vertex_buffer.vertex_base, instance_buffer.base);

	vkCmdEndRenderPass(commandBuffers[frame_index]);

	if (vkEndCommandBuffer(commandBuffers[frame_index]) != VK_SUCCESS) {
		throw "failed to record command buffers!";
	}
}

void make_CommandBuffers(VkDevice device, VkCommandPool command_pool, BufferAllocator& buffer_manager, Swapchain& swapchain) {
	int max_frames = frames_in_flight(swapchain);
	commandBuffers.resize(max_frames);

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = commandBuffers.length;

	if (vkAllocateCommandBuffers(device, &alloc_info, commandBuffers.data) != VK_SUCCESS) {
		throw "Failed to allocate command buffers";
	}
}

void make_SyncObjects(VkDevice device, Swapchain& swapchain) {
	for (int i = 0; i < swapchain.images.length; i++) swapchain.images_in_flight.append((VkFence)VK_NULL_HANDLE);

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &swapchain.image_available_semaphore[i]) != VK_SUCCESS 
		|| vkCreateSemaphore(device, &semaphoreInfo, nullptr, &swapchain.render_finished_semaphore[i]) != VK_SUCCESS 
		|| vkCreateFence(device, &fenceInfo, nullptr, &swapchain.in_flight_fences[i]) != VK_SUCCESS) {
			throw "failed to make semaphore!";
		}
	}
}

void make_DescriptorSetLayout(VkDevice device) {
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding bindings[2] = { uboLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 2;
	layoutInfo.pBindings = bindings;

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw "Failed to make descriptor set layout!";
	}
}

void make_DescriptorSets(VkDevice device, int frames_in_flight) {
	array<MAX_SWAP_CHAIN_IMAGES, VkDescriptorSetLayout> layouts(frames_in_flight);
	for (int i = 0; i < frames_in_flight; i++) layouts[i] = descriptorSetLayout;

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(frames_in_flight);
	allocInfo.pSetLayouts = layouts.data;

	descriptorSets.resize(frames_in_flight);
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data) != VK_SUCCESS) {
		throw "Failed to make descriptor sets!";
	}

	for (int i = 0; i < frames_in_flight; i++) {
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = uniform_buffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = texture_image_view;
		imageInfo.sampler = texture_sampler;

		VkWriteDescriptorSet descriptorWrites[2] = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
		descriptorWrites[0].pImageInfo = nullptr;
		descriptorWrites[0].pTexelBufferView = nullptr;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = nullptr;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);
	}
}

VkFormat find_supported_format(VkPhysicalDevice physical_device, slice<VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw "Failed to find supported format!";
}

VkFormat find_depth_format(VkPhysicalDevice physical_device) {
	array<3, VkFormat> candidates = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	return find_supported_format(physical_device, candidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void make_DepthResources(VkDevice device, VkPhysicalDevice physical_device, VkExtent2D extent) {
	VkFormat depthFormat = find_depth_format(physical_device);
	make_Image(device, physical_device, extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_image, depth_image_memory);
	depth_image_view = make_ImageView(device, depth_image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	//transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

}

VkDevice get_Device(RHI& rhi) {
	return rhi.device.device;
}

VkPhysicalDevice get_PhysicalDevice(RHI& rhi) {
	return rhi.device.physical_device;
}

VkInstance get_Instance(RHI& rhi) {
	return rhi.device.instance;
}

BufferAllocator& get_BufferAllocator(RHI& rhi) {
	return *rhi.buffer_manager;
}

VkCommandPool get_CommandPool(RHI& rhi) {
	return rhi.cmd_pool;
}

VkQueue get_GraphicsQueue(RHI& rhi) {
	return rhi.graphics_queue;
}

void begin_gpu_upload(RHI& rhi) {
	begin_buffer_upload(*rhi.buffer_manager);
}

void end_gpu_upload(RHI& rhi) {
	end_buffer_upload(*rhi.buffer_manager);
}

string_buffer vert_shader_code;
string_buffer frag_shader_code;

RHI* make_RHI(const VulkanDesc& desc, Window& window) {
	RHI* rhi = PERMANENT_ALLOC(RHI, { window, desc });

	VK_CHECK(volkInitialize());

	Device device;

	device.instance = make_Instance(desc);
	volkLoadInstance(device.instance);
	setup_debug_messenger(device.instance, desc, &device.debug_messenger);

	VkSurfaceKHR surface = make_Surface(device.instance, window);

	device.physical_device = pick_physical_devices(device.instance, surface);
	make_logical_devices(device, desc, surface);

	//a little wierd
	rhi->graphics_queue = device.graphics_queue;
	rhi->present_queue = device.present_queue;
	rhi->device = device;
	

	volkLoadDevice(device);

	rhi->swapchain = make_SwapChain(device, device.physical_device, rhi->window, surface);
	make_ImageViews(device, rhi->swapchain);

	rhi->cmd_pool = make_CommandPool(device, device.physical_device, surface);
	rhi->buffer_manager = make_BufferAllocator(*rhi);

	StagingQueue& staging_queue = get_StagingQueue(*rhi->buffer_manager);
	//make_StagingQueue(device, device.physical_device, rhi->cmd_pool, rhi->graphics_queue);
	begin_staging_cmds(staging_queue);

	return rhi;
}

static material_handle mat_handle;

void init_RHI(RHI* rhi, Assets& assets) {
	VkDevice device = get_Device(*rhi);
	VkPhysicalDevice physical_device = get_PhysicalDevice(*rhi);

	StagingQueue& staging_queue = get_StagingQueue(*rhi->buffer_manager);

	//vert_shader_code = read_file_or_fail(assets, "/shaders/cache/vert.spv");
	//frag_shader_code = read_file_or_fail(assets, "/shaders/cache/frag.spv");

	shader_handle shader = load_Shader(assets, "shaders/pbr.vert", "shaders/diffuse.frag");
	ShaderModules* shader_modules = get_shader_config(assets, shader, SHADER_INSTANCED);

	string_buffer texture_path = "C:/Users/User/Desktop/TopCCompiler/TopCompiler/Fernix/assets/level2/wet_street/Pebble_Wet_street_basecolor.jpg";

	MaterialDesc mat_desc{ shader };
	mat_channel3(mat_desc, "diffuse", glm::vec3(1.0), load_Texture(assets, "Aset_wood_log_L_rdeu3_4K_Albedo.jpg"));

	//make_TextureImage(&texture_image, &texture_image_memory, staging_queue, texture_path);

	mat_handle = make_Material(assets, mat_desc);

	make_RenderPass(device, physical_device, rhi->swapchain.imageFormat);
	make_DescriptorSetLayout(device);
	make_GraphicsPipeline(device, renderPass, rhi->swapchain.extent, *rhi->buffer_manager, shader_modules->vert, shader_modules->frag);

	make_DepthResources(device, physical_device, rhi->swapchain.extent);
	make_Framebuffers(device, rhi->swapchain);

	//make_StagingQueue(device, device.physical_device, rhi->cmd_pool, rhi->graphics_queue);
	begin_staging_cmds(staging_queue);

	int frames_in_flight = rhi->swapchain.images.length;

	make_UniformBuffers(device, physical_device, frames_in_flight);
	make_TextureImageView(device);
	texture_sampler = make_TextureSampler(device);
	make_DescriptorPool(device, frames_in_flight);
	make_DescriptorSets(device, frames_in_flight);
	
	upload_MeshData(assets, *rhi->buffer_manager);

	end_staging_cmds(staging_queue);
	
	make_CommandBuffers(device, rhi->cmd_pool, *rhi->buffer_manager, rhi->swapchain);
	make_SyncObjects(device, rhi->swapchain);

	rhi->desc.validation_layers = nullptr;
}

void destroy(VkDevice device, VkCommandPool command_pool, Swapchain& swapchain) {
	vkDestroyImageView(device, depth_image_view, nullptr);
	vkDestroyImage(device, depth_image, nullptr);
	vkFreeMemory(device, depth_image_memory, nullptr);

	vkFreeCommandBuffers(device, command_pool, commandBuffers.length, commandBuffers.data);

	for (int i = 0; i < swapchain.framebuffers.length; i++) {
		vkDestroyFramebuffer(device, swapchain.framebuffers[i], nullptr);
	}

	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);

	for (int i = 0; i < swapchain.image_views.length; i++) {
		vkDestroyImageView(device, swapchain.image_views[i], nullptr);
		vkDestroyBuffer(device, uniform_buffers[i], nullptr);
		vkFreeMemory(device, uniform_buffers_memory[i], nullptr);
	}

	vkDestroyDescriptorPool(device, descriptorPool, nullptr);

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void remakeSwapChain(RHI& rhi) {
	VkDevice device = rhi.device;
	VkPhysicalDevice physical_device = rhi.device.physical_device;
	Window& window = rhi.window;
	Swapchain& swapchain = rhi.swapchain;
	VkCommandPool cmd_pool = rhi.cmd_pool;
	BufferAllocator& buffer_manager = *rhi.buffer_manager;

	int width = 0, height = 0;

	window.get_framebuffer_size(&width, &height);

	while (width == 0 || height == 0) {
		window.get_framebuffer_size(&width, &height);
		window.wait_events();
	}

	vkDeviceWaitIdle(device);
	destroy(device, cmd_pool, swapchain);
	swapchain = make_SwapChain(device, physical_device, window, swapchain.surface);
	make_ImageViews(device, swapchain);

	make_RenderPass(device, physical_device, swapchain.imageFormat);
	make_GraphicsPipeline(device, renderPass, swapchain.extent, buffer_manager, make_ShaderModule(device, vert_shader_code), make_ShaderModule(device, frag_shader_code));
	make_DepthResources(device, physical_device, swapchain.extent);
	make_Framebuffers(device, swapchain);
	make_UniformBuffers(device, physical_device, frames_in_flight(swapchain));
	make_DescriptorPool(device, frames_in_flight(swapchain));
	make_DescriptorSets(device, frames_in_flight(swapchain));
	make_CommandBuffers(device, cmd_pool, buffer_manager, swapchain);
}

void vk_destroy(RHI& rhi) {
	Device& device = rhi.device;
	Swapchain& swapchain = rhi.swapchain;
	BufferAllocator* buffer_manager = rhi.buffer_manager;

	vkDeviceWaitIdle(device);

	destroy_sync_objects(device, swapchain);
	destroy(device, rhi.cmd_pool, swapchain);

	vkDestroySampler(device, texture_sampler, nullptr);
	vkDestroyImageView(device, texture_image_view, nullptr);

	vkDestroyImage(device, texture_image, nullptr);
	vkFreeMemory(device, texture_image_memory, nullptr);

	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

	destroy_BufferAllocator(buffer_manager);

	vkDestroyCommandPool(device, rhi.cmd_pool, nullptr);
	vkDestroyDevice(device, nullptr);

	destroy_validation_layers(device.instance, device.debug_messenger);

	destroy_Surface(device.instance, swapchain.surface);
	destroy_Instance(device.instance);
}

void update_UniformBuffer(VkDevice device, uint32_t currentImage, FrameData& frameData) {
	UniformBufferObject ubo = {};
	ubo.view = frameData.view_matrix;
	ubo.proj = frameData.proj_matrix;
	ubo.proj[1][1] *= -1;

	void* data;
	vkMapMemory(device, uniform_buffers_memory[currentImage], 0, sizeof(UniformBufferObject), 0, &data);
	memcpy(data, &ubo, sizeof(UniformBufferObject));
	vkUnmapMemory(device, uniform_buffers_memory[currentImage]);

}

void vk_draw_frame(RHI& rhi, FrameData& frameData) {
	VkDevice device = rhi.device;
	VkPhysicalDevice physical_device = rhi.device.physical_device;
	Swapchain& swapchain = rhi.swapchain;
	Window& window = rhi.window;
	BufferAllocator& buffer_allocator = *rhi.buffer_manager;

	uint current_frame = swapchain.current_frame;
	uint image_index = swapchain.image_index;

	vkWaitForFences(device, 1, &swapchain.in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

	VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, swapchain.image_available_semaphore[current_frame], VK_NULL_HANDLE, &image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		remakeSwapChain(rhi);
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw "Failed to acquire swapchain image!";
	}

	if (swapchain.images_in_flight[image_index] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &swapchain.images_in_flight[image_index], VK_TRUE, UINT64_MAX);
	}
	swapchain.images_in_flight[swapchain.image_index] = swapchain.in_flight_fences[swapchain.current_frame];

	update_UniformBuffer(device, image_index, frameData);
	record_CommandBuffers(device, buffer_allocator, swapchain, image_index);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { swapchain.image_available_semaphore[current_frame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[image_index];

	VkSemaphore signalSemaphores[] = { swapchain.render_finished_semaphore[current_frame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(device, 1, &swapchain.in_flight_fences[current_frame]);

	if (vkQueueSubmit(rhi.graphics_queue, 1, &submitInfo, swapchain.in_flight_fences[current_frame]) != VK_SUCCESS) {
		throw "Failed to submit draw command buffers!";
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &image_index;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(rhi.present_queue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		//framebufferResized = false;
		remakeSwapChain(rhi);
	}
	else if (result != VK_SUCCESS) {
		throw "failed to present swap chain image!";
	}

	swapchain.current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void destroy_RHI(RHI* rhi) {
	vk_destroy(*rhi);
}