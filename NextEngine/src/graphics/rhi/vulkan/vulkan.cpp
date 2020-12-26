#include <stdio.h>
#include <glm/gtc/matrix_transform.hpp>

#include "core/container/vector.h"
#include "core/container/tvector.h"
#include "core/memory/linear_allocator.h"


#include "vendor/stb_image.h"
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


//Resources
VertexBuffer vertex_buffer;
InstanceBuffer instance_buffer;

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

ArrayVertexInputs Vertex_attribute_descriptions() {
	return input_attributes(rhi.vertex_layouts, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);	
}

ArrayVertexBindings Vertex_binding_descriptors() {
	return input_bindings(rhi.vertex_layouts, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
}

VkFormat find_depth_format(VkPhysicalDevice, bool stencil);


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
	depthAttachment.format = find_depth_format(physical_device, false);
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

void make_DepthResources(VkDevice device, VkPhysicalDevice physical_device, VkExtent2D extent) {
	VkFormat depthFormat = find_depth_format(physical_device, false);
	make_alloc_Image(device, physical_device, extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depth_image, &depth_image_memory);
	depth_image_view = make_ImageView(device, depth_image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkDevice get_Device(RHI& rhi) {
	return rhi.device.device;
}

VkPhysicalDevice get_PhysicalDevice(RHI& rhi) {
	return rhi.device.physical_device;
}

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

	//todo make this tweakable
	u64 vertex_max_memory[VERTEX_LAYOUT_MAX] = { mb(500) };
	u64 index_max_memory[VERTEX_LAYOUT_MAX] = { mb(500) };
	u64 instance_max_memory[INSTANCE_LAYOUT_MAX] = { mb(0), mb(100), mb(5) };
	u64 ubo_max_memory[UBO_UPDATE_MODE_COUNT] = { mb(5), mb(5), mb(5) };

	//todo clean up function arguments
	rhi.staging_queue = make_StagingQueue(device, device.physical_device, rhi.transfer_cmd_pool, device[Queue_AsyncTransfer], device.queue_families[Queue_AsyncTransfer], device.queue_families[Queue_Graphics]);
	make_Layouts(rhi.vertex_layouts);
	make_VertexStreaming(rhi.vertex_streaming, device, device, rhi.staging_queue, rhi.vertex_layouts.vertex_layouts, vertex_max_memory,  index_max_memory);
	make_TextureAllocator(rhi.texture_allocator);
	make_UBOAllocator(rhi.ubo_allocator, device, ubo_max_memory);
	rhi.material_allocator.init();

	make_InstanceAllocator(render_thread.instance_allocator, device, device, &rhi.vertex_layouts.instance_layouts, instance_max_memory);
	make_CommandPool(render_thread.command_pool, device, Queue_Graphics, 15);
	make_CommandPool(rhi.background_graphics, device, Queue_Graphics, 3);

	DescriptorCount max_descriptor = {};
	max_descriptor.max_samplers = 100;
	max_descriptor.max_ubos = 100;
	max_descriptor.max_sets = 100;

	make_DescriptorPool(rhi.descriptor_pool, device, device, max_descriptor);

	make_DepthResources(device, device, rhi.swapchain.extent);
    make_RenderPass(device, device, rhi.swapchain.imageFormat);
	make_Framebuffers(device, rhi.swapchain);

	int frames_in_flight = get_frames_in_flight(rhi);

	register_wsi_pass(renderPass, swapchain.extent.width, swapchain.extent.height);

	begin_gpu_upload();
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

#include "core/atomic.h"

RenderPass begin_render_frame() {
	Swapchain& swapchain = rhi.swapchain;

    swapchain.current_frame = (swapchain.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;


	acquire_swapchain_image(swapchain);

	//todo: figure out why this is crashing
	for (DestructionJob& task : rhi.queued_for_destruction[swapchain.current_frame]) {
		task.func(task.data);
	}
    
    rhi.frame_index = rhi.swapchain.current_frame; //Broadcast frame index only after destruction
    
    TASK_MEMORY_BARRIER
    
    //todo eliminate distinction between frame and swapchain.current_frame

	rhi.queued_for_destruction[rhi.frame_index].clear();

	begin_frame(render_thread.command_pool, rhi.frame_index);
	begin_frame(render_thread.instance_allocator, rhi.frame_index);

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
