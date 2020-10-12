#include <stdio.h>
#include <glm/gtc/matrix_transform.hpp>

#include "core/container/vector.h"
#include "core/container/tvector.h"
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

#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/buffer.h"
#include "graphics/rhi/vulkan/pipeline.h"
#include "graphics/rhi/vulkan/shader.h"
#include "graphics/rhi/vulkan/device.h"
#include "graphics/rhi/vulkan/swapchain.h"
#include "graphics/rhi/vulkan/texture.h"
#include "graphics/rhi/vulkan/material.h"
#include "graphics/rhi/vulkan/command_buffer.h"
#include "graphics/rhi/vulkan/draw.h"
#include "graphics/rhi/vulkan/shader_access.h"
#include "graphics/rhi/vulkan/frame_buffer.h"

#include <shaderc/shaderc.h>

/*
struct RHI {
	Window& window;
	VulkanDesc desc;
	Device device;
	Swapchain swapchain;
	BufferAllocator* buffer_allocator;
	TextureStreaming* texture_allocator;
	PipelineCache* pipeline_cache;
	CommandPool graphics_cmd_pool;
	StagingQueue staging_queue;
	VkCommandPool transfer_cmd_pool;
	VkDescriptorPool descriptor_pool;
	uint waiting_on_transfer_frame;
};
*/

RHI::RHI() 
	: texture_allocator{ staging_queue }, 
	  vertex_streaming{ staging_queue }, 
	  material_allocator(device, device, descriptor_pool) {

}

thread_local RenderThreadResources render_thread;
RHI rhi;

VkDescriptorSetLayout descriptorSetLayout;

array<MAX_SWAPCHAIN_IMAGES, VkDescriptorSet> descriptorSets;

VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;

VkRenderPass renderPass;

array<MAX_SWAPCHAIN_IMAGES, VkCommandBuffer> commandBuffers;

VkDescriptorSet material_descriptor_set;
VkDescriptorSetLayout material_descriptor_layout;

//Resources
VertexBuffer vertex_buffer;
InstanceBuffer instance_buffer;

array<MAX_SWAPCHAIN_IMAGES, VkBuffer> uniform_buffers;
array<MAX_SWAPCHAIN_IMAGES, VkDeviceMemory> uniform_buffers_memory;

VkImage depth_image;
VkDeviceMemory depth_image_memory;
VkImageView depth_image_view;

tvector<glm::mat4> instances;

void upload_MeshData() {
	model_handle handle = load_Model("house.fbx");
	Model* model = get_Model(handle);

	instances.reserve(10 * 10);
	for (int x = 0; x < 10; x++) {
		for (int y = 0; y < 10; y++) {
			for (int z = 0; z < 1; z++) {
				glm::mat4 model(1.0);
				model = glm::translate(model, glm::vec3(x * 50, y * 20, z * 100));
				model = glm::scale(model, glm::vec3(10.0f));
				instances.append(model);
			}
		}
	}

	vertex_buffer = model->meshes[0].buffer[0];
	instance_buffer = frame_alloc_instance_buffer<glm::mat4>(INSTANCE_LAYOUT_MAT4X4, instances);

}

void make_UniformBuffers(VkDevice device, VkPhysicalDevice physical_device, int frames_in_flight) {
	VkDeviceSize bufferSize = sizeof(PassUBO);

	uniform_buffers.resize(frames_in_flight);
	uniform_buffers_memory.resize(frames_in_flight);

	for (int i = 0; i < frames_in_flight; i++) {
		make_Buffer(device, physical_device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniform_buffers[i], uniform_buffers_memory[i]);
	}
}

/*
VkDescriptorPool make_DescriptorPool(VkDevice device, int frames_in_flight) {
	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = frames_in_flight * 10;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = frames_in_flight * 20;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = frames_in_flight * 10;

	VkDescriptorPool result;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &result) != VK_SUCCESS) {
		throw "Failed to make descriptor pool!";

	}

	return result;
}
*/

string_buffer read_file_or_fail(string_view src) {
	string_buffer output;
	if (!io_readf(src, &output)) throw "Failed to load vertex shaders!";
	return output;
}

ArrayVertexInputs Vertex_attribute_descriptions() {
	return input_attributes(rhi.vertex_layouts, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);	
}

ArrayVertexBindings Vertex_binding_descriptors() {
	return input_bindings(rhi.vertex_layouts, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
}

void make_GraphicsPipeline(VkDevice device, VkRenderPass render_pass, VkExtent2D extent, VkShaderModule vert_shader, VkShaderModule frag_shader) {
	size_t before = temporary_allocator.occupied;

	//NEEDS REFERENCE
	auto binding_descriptors = Vertex_binding_descriptors();

	VkDescriptorSetLayout layouts[] = { descriptorSetLayout, material_descriptor_layout };

	VkPipelineDesc pipeline_desc;
	pipeline_desc.vert_shader = vert_shader;
	pipeline_desc.frag_shader = frag_shader;
	pipeline_desc.extent = extent;
	pipeline_desc.attribute_descriptions = Vertex_attribute_descriptions();
	pipeline_desc.binding_descriptions = binding_descriptors;
	pipeline_desc.descriptor_layouts = { layouts, 2 };
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

	VkSubpassDependency write_color_dependency = {};
	write_color_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	write_color_dependency.dstSubpass = 0;
	write_color_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	write_color_dependency.srcAccessMask = 0;
	write_color_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	write_color_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	//does this screen pass even need a depth buffer?
	VkSubpassDependency write_depth_dependency = {};
	write_depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	write_depth_dependency.dstSubpass = 0;
	write_depth_dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	write_depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	write_depth_dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	write_depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	write_depth_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


	VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };
	VkSubpassDependency dependencies[2] = { write_color_dependency, write_depth_dependency };
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw "Failed to make render pass!";
	}

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &write_color_dependency;
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

uint frames_in_flight(Swapchain& swapchain) {
	return swapchain.images.length;
}

uint get_frames_in_flight(RHI& rhi) {
	return rhi.swapchain.images.length;
}

void record_CommandBuffers(VkCommandBuffer cmd_buffer, Swapchain& swapchain, int32_t image_index) {
	uint frame_index = rhi.frame_index;

	VkRenderPassBeginInfo render_pass_info = {};

	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = renderPass;
	render_pass_info.framebuffer = swapchain.framebuffers[image_index];
	render_pass_info.renderArea.offset = { 0,0 };
	render_pass_info.renderArea.extent = swapchain.extent;

	VkClearValue clear_colors[2];
	clear_colors[0].color = { 0.1f, 0.1f, 0.1f, 1.0f };
	clear_colors[1].depthStencil = { 1, 0 };

	render_pass_info.clearValueCount = 2;
	render_pass_info.pClearValues = clear_colors;

	//todo move into TransferSystem
	vkCmdBeginRenderPass(cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	/*
	vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertex_buffers, offsets);
	vkCmdBindVertexBuffers(commandBuffers[i], 1, 1, instance_buffers, offsets);
	vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
	*/
	
	VkDescriptorSet descriptor_sets[] = { descriptorSets[frame_index], material_descriptor_set };

	bind_vertex_buffer(rhi.vertex_streaming, cmd_buffer, VERTEX_LAYOUT_DEFAULT);
	bind_instance_buffer(render_thread.instance_allocator, cmd_buffer, INSTANCE_LAYOUT_MAT4X4);
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, descriptor_sets, 0, nullptr);

	vkCmdDrawIndexed(cmd_buffer, vertex_buffer.length, instance_buffer.length, vertex_buffer.index_base, vertex_buffer.vertex_base, instance_buffer.base);

	vkCmdEndRenderPass(cmd_buffer);
}

void make_DescriptorSetLayout(VkDevice device) {
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding bindings[2] = { uboLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = bindings;

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw "Failed to make descriptor set layout!";
	}
}

void make_DescriptorSets(VkDevice device, VkDescriptorPool pool, int frames_in_flight) {
	array<MAX_SWAPCHAIN_IMAGES, VkDescriptorSetLayout> layouts(frames_in_flight);
	for (int i = 0; i < frames_in_flight; i++) layouts[i] = descriptorSetLayout;

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
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
		bufferInfo.range = sizeof(PassUBO);

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

		vkUpdateDescriptorSets(device, 1, descriptorWrites, 0, nullptr);
	}
}


void make_DepthResources(VkDevice device, VkPhysicalDevice physical_device, VkExtent2D extent) {
	VkFormat depthFormat = find_depth_format(physical_device);
	make_alloc_Image(device, physical_device, extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depth_image, &depth_image_memory);
	depth_image_view = make_ImageView(device, depth_image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	//transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

}

VkDevice get_Device(RHI& rhi) {
	return rhi.device.device;
}

VkPhysicalDevice get_PhysicalDevice(RHI& rhi) {
	return rhi.device.physical_device;
}

//VkDescriptorPool get_DescriptorPool(RHI& rhi) {
//	return rhi.descriptor_pool;
//}

//VkInstance get_Instance(RHI& rhi) {
//	return rhi.device.instance;
//}

//BufferAllocator& get_BufferAllocator(RHI& rhi) {
//	return *rhi.buffer_allocator;
//}

//TextureStreaming& get_TextureAllocator(RHI& rhi) {
//	return *rhi.texture_allocator;
//}

//VkQueue get_Queue(RHI& rhi, QueueType type) {
//	return rhi.device[type];
//}

uint get_active_frame(RHI& rhi) {
	return rhi.swapchain.current_frame;
}

string_buffer vert_shader_code;
string_buffer frag_shader_code;

void make_RHI(const VulkanDesc& desc, Window& window) {
	VK_CHECK(volkInitialize());

	VK_CHECK(volkInitialize());

	Device& device = rhi.device;
	Swapchain& swapchain = rhi.swapchain;

	VkSurfaceKHR surface = make_Device(device, desc, window);

	rhi.window = &window;
	rhi.shader_compiler = shaderc_compiler_initialize();

	make_Swapchain(rhi.swapchain, device, window, surface);
	make_SyncObjects(swapchain);

	make_vk_CommandPool(rhi.transfer_cmd_pool, device, Queue_AsyncTransfer);

	u64 vertex_max_memory[VERTEX_LAYOUT_COUNT] = { mb(50) };
	u64 index_max_memory[VERTEX_LAYOUT_COUNT] = { mb(50) };
	u64 instance_max_memory[INSTANCE_LAYOUT_COUNT] = { mb(0), mb(5), mb(5) };
	u64 ubo_max_memory[UBO_UPDATE_MODE_COUNT] = { mb(5), mb(5), mb(5) };

	//todo clean up function arguments
	rhi.staging_queue = make_StagingQueue(device, device.physical_device, rhi.transfer_cmd_pool, device[Queue_AsyncTransfer], device.queue_families[Queue_AsyncTransfer], device.queue_families[Queue_Graphics]);
	make_Layouts(rhi.vertex_layouts);
	make_VertexStreaming(rhi.vertex_streaming, device, device, rhi.staging_queue, rhi.vertex_layouts.vertex_layouts, vertex_max_memory,  index_max_memory);
	make_TextureAllocator(rhi.texture_allocator);
	make_UBOAllocator(rhi.ubo_allocator, device, ubo_max_memory);
	rhi.material_allocator.init();

	make_InstanceAllocator(render_thread.instance_allocator, device, device, &rhi.vertex_layouts.instance_layouts, instance_max_memory);
	make_CommandPool(render_thread.command_pool, device, Queue_Graphics, 10);
	make_CommandPool(rhi.background_graphics, device, Queue_Graphics, 3);

	DescriptorCount max_descriptor = {};
	max_descriptor.max_samplers = 50;
	max_descriptor.max_ubos = 50;
	max_descriptor.max_sets = 50;

	make_DescriptorPool(rhi.descriptor_pool, device, device, max_descriptor);

	make_RenderPass(device, device, rhi.swapchain.imageFormat);
	make_DescriptorSetLayout(device);
	//make_GraphicsPipeline(device, renderPass, rhi.swapchain.extent, shader_modules->vert, shader_modules->frag);

	make_DepthResources(device, device, rhi.swapchain.extent);
	make_Framebuffers(device, rhi.swapchain);

	int frames_in_flight = get_frames_in_flight(rhi);

	make_UniformBuffers(device, device, frames_in_flight);
	make_DescriptorSets(device, rhi.descriptor_pool, frames_in_flight);

	register_wsi_pass(renderPass, swapchain.extent.width, swapchain.extent.height);

	begin_gpu_upload();
}

//todo all of init RHI and the adhoc initialization has to be removed
//this is only needed for the bootstrapping phase before the other systems come 
//back online again

void init_RHI() {	
	VkDevice device = get_Device(rhi);
	VkPhysicalDevice physical_device = get_PhysicalDevice(rhi);
}

void destroy(VkDevice device, Swapchain& swapchain) {
	vkDestroyImageView(device, depth_image_view, nullptr);
	vkDestroyImage(device, depth_image, nullptr);
	vkFreeMemory(device, depth_image_memory, nullptr);

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

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void remakeSwapChain(RHI& rhi) {
	VkDevice device = rhi.device;
	VkPhysicalDevice physical_device = rhi.device.physical_device;
	Swapchain& swapchain = rhi.swapchain;

	int width = 0, height = 0;

	rhi.window->get_framebuffer_size(&width, &height);

	while (width == 0 || height == 0) {
		rhi.window->get_framebuffer_size(&width, &height);
		rhi.window->wait_events();
	}

	vkDeviceWaitIdle(device);

	//todo MASSIVE RESOURCE LEAK!
	//todo if swapchain image format changes, the graphics pipelines also need to be recompiled

	make_Swapchain(swapchain, rhi.device, *rhi.window, swapchain.surface);


	//make_RenderPass(device, physical_device, swapchain.imageFormat);
	//make_GraphicsPipeline(device, renderPass, swapchain.extent, make_ShaderModule(vert_shader_code), make_ShaderModule(frag_shader_code));
	make_DepthResources(device, physical_device, swapchain.extent);
	make_Framebuffers(device, swapchain);
}

void vk_destroy() {
	Device& device = rhi.device;
	Swapchain& swapchain = rhi.swapchain;

	vkDeviceWaitIdle(device);

	shaderc_compiler_release(rhi.shader_compiler);

	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(device, rhi.descriptor_pool, nullptr);

	destroy_VertexStreaming(rhi.vertex_streaming);

	destroy_CommandPool(render_thread.command_pool);
	vkDestroyCommandPool(device, rhi.transfer_cmd_pool, nullptr);

	destroy_Swapchain(rhi.swapchain);

	vkDestroyDevice(device, nullptr);

	destroy_Device(device);
}

#include "graphics/pass/render_pass.h"

void update_UniformBuffer(VkDevice device, uint32_t currentImage, Viewport& viewport) {
	PassUBO ubo = {};
	ubo.view = viewport.view;
	ubo.proj = viewport.proj;
	ubo.proj[1][1] *= -1;

	void* data;
	vkMapMemory(device, uniform_buffers_memory[currentImage], 0, sizeof(PassUBO), 0, &data);
	memcpy(data, &ubo, sizeof(PassUBO));
	vkUnmapMemory(device, uniform_buffers_memory[currentImage]);

}

void vk_begin_frame() {
	uint current_frame = rhi.swapchain.current_frame;

	vkWaitForFences(rhi.device, 1, &rhi.swapchain.in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

	begin_frame(render_thread.command_pool, current_frame);
}

//todo move to vk_swapchain
void acquire_swapchain_image(Swapchain& swapchain) {
	VkDevice device = swapchain.device;

	//printf("====== WAITING ON FRAME %i =====\n", swapchain.current_frame);
	vkWaitForFences(device, 1, &swapchain.in_flight_fences[swapchain.current_frame], VK_TRUE, UINT64_MAX);

	VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, swapchain.image_available_semaphore[swapchain.current_frame], VK_NULL_HANDLE, &swapchain.image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		remakeSwapChain(rhi);
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw "Failed to acquire swapchain image!";
	}

	if (swapchain.images_in_flight[swapchain.image_index] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &swapchain.images_in_flight[swapchain.image_index], VK_TRUE, UINT64_MAX);
	}
	swapchain.images_in_flight[swapchain.image_index] = swapchain.in_flight_fences[swapchain.current_frame];
}

void queue_for_destruction(void* data, void(*func)(void*)) {
	rhi.queued_for_destruction[rhi.frame_index].append({data, func});
}

void present_swapchain_image(Swapchain& swapchain) {
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &swapchain.render_finished_semaphore[swapchain.current_frame]; 

	VkSwapchainKHR swapChains[] = { swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &swapchain.image_index;
	presentInfo.pResults = nullptr;

	VkResult result = vkQueuePresentKHR(rhi.device.present_queue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		//framebufferResized = false;
		remakeSwapChain(rhi);
	}
	else if (result != VK_SUCCESS) {
		throw "failed to present swap chain image!";
	}
}

/*
void vk_end_frame() {
	VkDevice device = rhi.device;
	VkPhysicalDevice physical_device = rhi.device.physical_device;
	Swapchain& swapchain = rhi.swapchain;
	//BufferAllocator& buffer_allocator = *rhi.buffer_allocator;
	//TextureStreaming& texture_allocator = *rhi.texture_allocator;

	acquire_swapchain_image(rhi.swapchain);

	{
		CommandPool& graphics_cmd_pool = render_thread.command_pool;
		//StagingQueue& staging_queue = rhi.trastaging_queue;

		begin_frame(graphics_cmd_pool, rhi.frame_index);

		VkCommandBuffer cmd_buffer = begin_recording(graphics_cmd_pool);

		//update_UniformBuffer(device, swapchain.current_frame, frameData);
		record_CommandBuffers(cmd_buffer, swapchain, swapchain.image_index);

		end_recording(graphics_cmd_pool, cmd_buffer);
	
		QueueSubmitInfo submit_info = {};
		submit_info.completion_fence = swapchain.in_flight_fences[swapchain.current_frame];

		wait_on_transfer(rhi.waiting_on_transfer_frame, cmd_buffer, submit_info);
		
		submit_all_cmds(graphics_cmd_pool, submit_info);

		queue_wait_semaphore(submit_info, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, swapchain.image_available_semaphore[swapchain.current_frame]);
		queue_signal_semaphore(submit_info, swapchain.render_finished_semaphore[swapchain.current_frame]);

		vkResetFences(device, 1, &swapchain.in_flight_fences[swapchain.current_frame]);

		queue_submit(rhi.device, Queue_Graphics, submit_info);
	}

	swapchain.current_frame = (swapchain.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}*/

void begin_gpu_upload() {
	rhi.waiting_on_transfer_frame = begin_staging_cmds(rhi.staging_queue);
	begin_vertex_buffer_upload(rhi.vertex_streaming);

	begin_frame(rhi.background_graphics, rhi.staging_queue.frame_index);
}

//todo add sychronization
void end_gpu_upload() {
	end_vertex_buffer_upload(rhi.vertex_streaming);
	end_staging_cmds(rhi.staging_queue);


	QueueSubmitInfo info = {};

	queue_wait_timeline_semaphore(info, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, rhi.staging_queue.wait_on_transfer, rhi.waiting_on_transfer_frame);
	queue_signal_timeline_semaphore(info, rhi.staging_queue.wait_on_transfer, ++rhi.waiting_on_transfer_frame);

	rhi.staging_queue.value_wait_on_transfer++;

	
	{
		VkCommandBuffer cmd_buffer = begin_recording(rhi.background_graphics);
		transfer_image_ownership(rhi.texture_allocator, cmd_buffer);
		transfer_vertex_ownership(rhi.vertex_streaming, cmd_buffer);
		
		end_recording(rhi.background_graphics, cmd_buffer);
	
		auto& cmds = rhi.background_graphics.submited[rhi.background_graphics.frame_index];
		
		for (int i = cmds.length - 1; i > 0; i--) {
			cmds[i] = cmds[i - 1];
		}

		cmds[0] = cmd_buffer;		
	}

	submit_all_cmds(rhi.background_graphics, info);

	queue_submit(rhi.device, rhi.device.graphics_queue, info);
	
}

RenderPass begin_render_frame() {
	Swapchain& swapchain = rhi.swapchain;
	rhi.frame_index = rhi.swapchain.current_frame;

	acquire_swapchain_image(swapchain);

	//todo: figure out why this is crashing
	//for (Task& task : rhi.queued_for_destruction[rhi.frame_index]) {
	//	task.func(task.data);
	//}

	rhi.queued_for_destruction[rhi.frame_index].clear();

	begin_frame(render_thread.command_pool, rhi.frame_index);
	begin_frame(render_thread.instance_allocator, rhi.frame_index);

	/*{
		CommandBuffer& cmd_buffer = begin_draw_cmds();
		transfer_image_ownership(rhi.texture_allocator, cmd_buffer);
		transfer_vertex_ownership(rhi.vertex_streaming, cmd_buffer);
		end_draw_cmds(cmd_buffer);
	}*/

	CommandBuffer& cmd_buffer = begin_draw_cmds();



	uint frame_index = rhi.frame_index;

	VkRenderPassBeginInfo render_pass_info = {};

	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = renderPass;
	render_pass_info.framebuffer = swapchain.framebuffers[swapchain.image_index];
	render_pass_info.renderArea.offset = { 0,0 };
	render_pass_info.renderArea.extent = swapchain.extent;

	VkClearValue clear_colors[2] = {};
	clear_colors[0].color = { 0.4f, 0.4f, 0.9f, 1.0f };
	clear_colors[1].depthStencil = { 1, 0 };

	render_pass_info.clearValueCount = 2;
	render_pass_info.pClearValues = clear_colors;

	//todo move into TransferSystem
	vkCmdBeginRenderPass(cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	Viewport viewport = {};
	viewport.width = swapchain.extent.width;
	viewport.height = swapchain.extent.height;

	VkViewport vk_viewport = {};
	vk_viewport.x = viewport.x;
	vk_viewport.y = viewport.y;
	vk_viewport.width = viewport.width;
	vk_viewport.height = viewport.height;

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = vk_viewport.width;
	scissor.extent.height = vk_viewport.height;

	vkCmdSetViewport(cmd_buffer, 0, 1, &vk_viewport);
	vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

	return { RenderPass::Screen, RenderPass::Color, 0, viewport, cmd_buffer };
}



void destroy_RHI() {
	vk_destroy();
}