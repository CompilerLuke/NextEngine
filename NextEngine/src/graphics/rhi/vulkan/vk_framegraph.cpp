#include "engine/core.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/draw.h"
#include "graphics/rhi/vulkan/swapchain.h"
#include "graphics/rhi/vulkan/frame_buffer.h"
#include "graphics/assets/assets_store.h"

AttachmentDesc& add_color_attachment(FramebufferDesc& desc, texture_handle* handle) {
	AttachmentDesc attachment;
	attachment.tex_id = handle;
    attachment.usage |= TextureUsage::ColorAttachment;

	desc.color_attachments.append(attachment);
	return desc.color_attachments.last();
}

AttachmentDesc& add_depth_attachment(FramebufferDesc& desc, texture_handle* handle, DepthBufferFormat format) {
	AttachmentDesc* attachment = TEMPORARY_ALLOC(AttachmentDesc);
	attachment->tex_id = handle;
    attachment->usage |= TextureUsage::DepthAttachment;
	desc.depth_buffer = format;

	desc.depth_attachment = attachment;
	return *attachment;
}

struct Framegraph {
	uint framebuffer_count = RenderPass::PassCount;

	VkFramebuffer framebuffer[MAX_FRAMEBUFFER]; //[MAX_FRAMES_IN_FLIGHT] , determine wether it's necessary to triple buffer!
	VkEvent render_pass_complete[MAX_FRAMES_IN_FLIGHT][MAX_FRAMEBUFFER];
	VkRenderPass render_pass[MAX_FRAMEBUFFER];
	RenderPassInfo info[MAX_FRAMEBUFFER];
	CommandBuffer* submitted_cmd_buffer[MAX_FRAMEBUFFER];

	array<MAX_FRAMEBUFFER, RenderPass::ID> render_pass_order;
};

void add_dependency(FramebufferDesc& desc, Stage stage, RenderPass::ID id, TextureAspect aspect) {
    desc.dependency.append({ stage, id, aspect });
}

static Framegraph framegraph;

uint render_pass_samples_by_id(RenderPass::ID pass_id) {
    return framegraph.info[pass_id].attachments[0].samples; //todo may not be correct in all cases
}

render_pass_handle render_pass_by_id(RenderPass::ID pass_id) {
	return { (u64)framegraph.render_pass[pass_id] };
}

RenderPass::Type render_pass_type(RenderPass::ID pass_id, uint subpass) {
	return framegraph.info[pass_id].types[subpass];
}
 
Viewport render_pass_viewport_by_id(RenderPass::ID pass_id) {
	Viewport viewport = {};
	viewport.width = framegraph.info[pass_id].width;
	viewport.height = framegraph.info[pass_id].height;
	return viewport;
}

ENGINE_API uint render_pass_num_color_attachments_by_id(RenderPass::ID id, uint subpass) {
	uint count = 0;
	for (Attachment& desc : framegraph.info[id].attachments) {
		if (desc.aspect == VK_IMAGE_ASPECT_COLOR_BIT) count++;
	}
	return count;
}

const uint MAX_DEP_CHAIN_LENGTH = 10;

uint compute_chain_length(RenderPass::ID pass, array<MAX_DEP_CHAIN_LENGTH, tvector<RenderPass::ID>>& ordered_by_depth) {
	RenderPassInfo& info = framegraph.info[pass];
	
	if (info.dependency_chain_length != -1) return info.dependency_chain_length;

	int longest_chain = 0;

	for (Dependency dependency : info.dependencies) {
		uint dep_chain_length = compute_chain_length(dependency.id, ordered_by_depth);
		longest_chain = max(longest_chain, dep_chain_length + 1);
	}

	if (longest_chain >= ordered_by_depth.length) {
		ordered_by_depth.resize(longest_chain + 1);
	}

	info.dependency_chain_length = longest_chain;
	printf("INSERTED %i at %i\n", pass, longest_chain);
	ordered_by_depth[longest_chain].append(pass);

	return longest_chain;
}

void topoligical_sort_render_passes() {
	array<MAX_DEP_CHAIN_LENGTH, tvector<RenderPass::ID>> ordered_by_depth;

	for (uint i = 0; i < MAX_FRAMEBUFFER; i++) {
		if (framegraph.render_pass[i] == VK_NULL_HANDLE) continue;

		compute_chain_length((RenderPass::ID)i, ordered_by_depth);
	}

	auto& render_pass_order = framegraph.render_pass_order;
	render_pass_order.clear();

	for (tvector<RenderPass::ID>& ordered : ordered_by_depth) {
		memcpy(render_pass_order.data + render_pass_order.length, ordered.data, sizeof(RenderPass::ID) * ordered.length);
		render_pass_order.length += ordered.length;

		assert(render_pass_order.length < MAX_FRAMEBUFFER);
	}

	printf("===== RENDERING ORDER =====\n");

	for (RenderPass::ID id : render_pass_order) {
		printf("%i\n", id);
	}
}

void build_framegraph() {
	topoligical_sort_render_passes();
}

void make_Framebuffer(RenderPass::ID id, FramebufferDesc& desc, slice<SubpassDesc> subpasses) {
	VkDevice device = rhi.device.device;
	VkPhysicalDevice physical_device = rhi.device.physical_device;
	VkRenderPass render_pass = make_RenderPass(device, physical_device, desc, subpasses);

	framegraph.render_pass[id] = render_pass;
	
	RenderPassInfo& info = framegraph.info[id];

	info.width  = desc.width;
	info.height = desc.height;
	info.dependencies = (slice<Dependency>)desc.dependency;

	framegraph.framebuffer[id] = make_Framebuffer(device, physical_device, render_pass, desc, info);

	for (SubpassDesc& subpass : subpasses) {
		info.types.append(subpass.color_attachments.length == 0 ? RenderPass::Depth : RenderPass::Color);
	}

	for (uint frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
		framegraph.render_pass_complete[frame][id] = make_Event(device);
	}
}

void make_Framebuffer(RenderPass::ID id, FramebufferDesc& desc) {
	SubpassDesc subpass = {};
	
	for (uint i = 0; i < desc.color_attachments.length; i++) {
		subpass.color_attachments.append(i);
	}

	subpass.depth_attachment = desc.depth_buffer != DepthBufferFormat::None;

	make_Framebuffer(id, desc, subpass);
}

RenderPass::ID make_Framebuffer(FramebufferDesc& desc) {
    RenderPass::ID id = (RenderPass::ID)framegraph.framebuffer_count++;
    make_Framebuffer(id, desc);
    return id;
}

RenderPass::ID make_Framebuffer(FramebufferDesc& desc, slice<SubpassDesc> subpasses) {
    RenderPass::ID id = (RenderPass::ID)framegraph.framebuffer_count++;
    make_Framebuffer(id, desc, subpasses);
    return id;
}

void make_wsi_pass(slice<Dependency> dependency) {
	framegraph.info[RenderPass::Screen].dependencies = dependency;
	framegraph.info[RenderPass::Screen].types.append(RenderPass::Color);
    
    Attachment attachment;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    
    framegraph.info[RenderPass::Screen].attachments.append(attachment);
	//todo add support for dependencies

}

void register_wsi_pass(VkRenderPass render_pass, uint width, uint height) {
	framegraph.info[RenderPass::Screen].types.append(RenderPass::Color);
	framegraph.info[RenderPass::Screen].width = width;
	framegraph.info[RenderPass::Screen].height = height;
	framegraph.render_pass[RenderPass::Screen] = render_pass;
    
    Attachment attachment;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    
    framegraph.info[RenderPass::Screen].attachments.append(attachment);
}

RenderPass::Type render_pass_type_by_id(RenderPass::ID id, uint subpass) {
	const RenderPassInfo& info = framegraph.info[id];
	return info.types[subpass];
}

RenderPass begin_render_pass(RenderPass::ID id, glm::vec4 clear_color) {
	const RenderPassInfo& view = framegraph.info[id];

	Viewport viewport = {};
	viewport.width = view.width;
	viewport.height = view.height;

	VkRenderPass vk_render_pass = framegraph.render_pass[id];
	
	CommandBuffer* multiple_this_frame = framegraph.submitted_cmd_buffer[id];
	CommandBuffer& cmd_buffer = multiple_this_frame ? *multiple_this_frame : begin_draw_cmds();

	VkRenderPassBeginInfo render_pass_info = {};

	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = vk_render_pass;
	render_pass_info.framebuffer = framegraph.framebuffer[id]; //[rhi.swapchain.image_index]
	render_pass_info.renderArea.offset = { (int32_t)viewport.x, (int32_t)viewport.y };
	render_pass_info.renderArea.extent = { viewport.width, viewport.height };

	//todo add customization
	VkClearValue clear_colors[MAX_ATTACHMENT] = {};
    
    uint offset = 0;
    for (uint i = 0; i < view.attachments.length; i++) {
        if (view.attachments[i].aspect == VK_IMAGE_ASPECT_DEPTH_BIT) clear_colors[offset].depthStencil = { 1, 0};
        else clear_colors[offset].color = { clear_color.x, clear_color.y, clear_color.z, clear_color.w };
        
        if (view.attachments[i].samples != VK_SAMPLE_COUNT_1_BIT) offset += 2;
        else offset++;
    }
    
	render_pass_info.clearValueCount = offset;
	render_pass_info.pClearValues = clear_colors;

	//todo merge into one call
	array<10, Dependency> dependencies = framegraph.info[id].dependencies;
	

	//for (Dependency& dependency : dependencies) {
	//	VkEvent wait_on_event = framegraph.render_pass_complete[rhi.frame_index][dependency.id];
	//
	//	vkCmdWaitEvents(cmd_buffer, 1, &wait_on_event, )
	//}

	//vkCmdResetEvent(cmd_buffer, framegraph.render_pass_complete[rhi.frame_index][id], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	vkCmdBeginRenderPass(cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	//todo make state built into pipeline
	VkViewport vk_viewport = {};
	vk_viewport.x = viewport.x;
	vk_viewport.y = viewport.y;
	vk_viewport.width = viewport.width;
	vk_viewport.height = viewport.height;
	vk_viewport.minDepth = 0.0f;
	vk_viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = vk_viewport.width;
	scissor.extent.height = vk_viewport.height;

	vkCmdSetViewport(cmd_buffer, 0, 1, &vk_viewport);
	vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

	cmd_buffer.render_pass = id;


	return { id, view.types[0], {(u64)vk_render_pass}, viewport, &cmd_buffer };
}

void generate_mips_after_render_pass(VkCommandBuffer cmd_buffer, RenderPassInfo& info, Attachment& attachment) {
	int mip_width = info.width;
	int mip_height = info.height;

	VkImageLayout final_layout = attachment.final_layout; 

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = attachment.image;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrier);

	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.subresourceRange.baseMipLevel = 1;
	barrier.subresourceRange.levelCount = attachment.mips - 1;

	vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, 0, 1, &barrier);

	for (uint i = 1; i < attachment.mips; i++) {
		if (i != 1) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, 0, 1, &barrier);
		}

		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0,0,0 };
		blit.srcOffsets[1] = { mip_width, mip_height, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.srcSubresource.mipLevel = i - 1;

		if (mip_width > 1) mip_width /= 2;
		if (mip_height > 1) mip_height /= 2;

		blit.dstOffsets[0] = { 0,0,0 };
		blit.dstOffsets[1] = { mip_width, mip_height, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		blit.dstSubresource.mipLevel = i;

		vkCmdBlitImage(cmd_buffer, attachment.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, attachment.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
	}


	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; 
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = final_layout;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = attachment.mips - 1;

	if (final_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 0, 0, 1, &barrier); //todo when compiling the framegraph we need to figure out the image dependencies
	}

	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.subresourceRange.baseMipLevel = attachment.mips - 1;
	barrier.subresourceRange.levelCount = 1;

	if (final_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 0, 0, 1, &barrier); //todo when compiling the framegraph we need to figure out the image dependencies
	}
}

void end_render_pass(RenderPass& render_pass) {
	vkCmdEndRenderPass(*render_pass.cmd_buffer);

	VkEvent event = framegraph.render_pass_complete[rhi.frame_index][render_pass.id];
	if (event) vkCmdSetEvent(*render_pass.cmd_buffer, event, render_pass.type == RenderPass::Color ?
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

    RenderPassInfo& info = framegraph.info[render_pass.id];
	for(Attachment& attachment : info.attachments) {
		if (attachment.mips > 1) generate_mips_after_render_pass(*render_pass.cmd_buffer, info, attachment);
	}

	assert(render_pass.cmd_buffer->cmd_buffer != VK_NULL_HANDLE);
	framegraph.submitted_cmd_buffer[render_pass.id] = TEMPORARY_ALLOC(CommandBuffer, *render_pass.cmd_buffer);

	//end_draw_cmds(render_pass.cmd_buffer);
}

void submit_framegraph() {
	for (RenderPass::ID id : framegraph.render_pass_order) {
		if (!framegraph.submitted_cmd_buffer[id]) continue;

		end_draw_cmds(*framegraph.submitted_cmd_buffer[id]);
		framegraph.submitted_cmd_buffer[id] = nullptr;

		//assert(cmd_buffer.cmd_buffer, "Never completed render pass!");
	}	
}

void next_subpass(RenderPass& render_pass) {
	uint subpass = ++render_pass.cmd_buffer->subpass;
	render_pass.type = framegraph.info[render_pass.id].types[subpass];

	vkCmdNextSubpass(*render_pass.cmd_buffer, VK_SUBPASS_CONTENTS_INLINE); //todo somewhat strange that command buffer
    // and render pass struct overlap
}

void end_render_frame(RenderPass& render_pass) {
	CommandPool& graphics_cmd_pool = render_thread.command_pool;
	Swapchain& swapchain = rhi.swapchain;

	vkCmdEndRenderPass(*render_pass.cmd_buffer);
	//end_draw_cmds(render_pass.cmd_buffer);
	framegraph.submitted_cmd_buffer[RenderPass::Screen] = render_pass.cmd_buffer;

	uint current_frame = rhi.frame_index;
	uint last_frame = current_frame == 0 ? MAX_FRAMES_IN_FLIGHT - 1 : current_frame - 1;

	QueueSubmitInfo submit_info = {};
	submit_info.completion_fence = swapchain.in_flight_fences[current_frame];


	//todo HACK!, I need the wait on transfer to occur before everything else!
	//std::swap(graphics_cmd_pool.submited[rhi.frame_index][0], graphics_cmd_pool.submited[rhi.frame_index].last());

	submit_framegraph();
	submit_all_cmds(graphics_cmd_pool, submit_info);

	//static bool first_frame = true;

	//0x83d4ee000000000b[]

	//if (!first_frame) queue_wait_semaphore(submit_info, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, swapchain.render_finished_semaphore2[last_frame]); //wait for last frame, as we have a single color images
	transfer_queue_dependencies(rhi.staging_queue, submit_info, rhi.waiting_on_transfer_frame);
	queue_wait_semaphore(submit_info, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, swapchain.image_available_semaphore[current_frame]);
	//queue_signal_semaphore(submit_info, swapchain.render_finished_semaphore2[current_frame]);
	queue_signal_semaphore(submit_info, swapchain.render_finished_semaphore[current_frame]);

	//printf("Last frame : %i, Current frame : %i", last_frame, current_frame);
	//printf("Signalling semaphore %p\n", swapchain.render_finished_semaphore2[current_frame]);
	//printf("Waiting on semaphore %p\n", swapchain.render_finished_semaphore2[last_frame]);

	vkResetFences(rhi.device, 1, &swapchain.in_flight_fences[current_frame]);

	queue_submit(rhi.device, Queue_Graphics, submit_info);

	present_swapchain_image(rhi.swapchain);
}

VkFormat find_depth_format(VkPhysicalDevice physical_device, bool stencil);

VkFormat find_color_format(VkPhysicalDevice device, AttachmentDesc& desc) {
    return to_vk_image_format(desc.format, TextureAspect::Color, desc.num_channels);
}

VkSampleCountFlagBits find_sample_count(VkSampleCountFlagBits max, uint count) {
    assert((count & (count-1)) == 0);
    return count > max ? max : (VkSampleCountFlagBits)count;
}

const uint MAX_SUBPASSES = 5;

VkAttachmentLoadOp to_vk_load_op(LoadOp op) {
	VkAttachmentLoadOp ops[] = {VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_DONT_CARE};
	return ops[(uint)op];
}

VkAttachmentStoreOp to_vk_store_op(StoreOp op) {
	VkAttachmentStoreOp ops[] = { VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_STORE_OP_DONT_CARE };
	return ops[(uint)op];
}

VkSampleCountFlagBits get_max_usable_sample_count(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
    
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

/*
			color_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
			color_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			color_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
*/

VkRenderPass make_RenderPass(VkDevice device, VkPhysicalDevice physical_device, FramebufferDesc& desc, slice<SubpassDesc> subpasses) {
	VkSubpassDescription subpass_description[MAX_SUBPASSES] = {};
    array<MAX_ATTACHMENT * 2, VkAttachmentDescription> attachments_desc = {};
    array<MAX_ATTACHMENT, VkAttachmentReference> resolve_attachment_ref = {};
    
	tvector<VkSubpassDependency> dependencies;
    VkSampleCountFlagBits max_samples = get_max_usable_sample_count(physical_device);

	for (uint i = 0; i < desc.color_attachments.length; i++) {
		AttachmentDesc& attach = desc.color_attachments[i];

		VkFormat format = find_color_format(physical_device, attach);
        VkSampleCountFlagBits samples = find_sample_count(max_samples, attach.num_samples);

		VkAttachmentDescription color_attachment_desc = {};
		color_attachment_desc.format = format;
        color_attachment_desc.samples = samples;
		color_attachment_desc.loadOp = to_vk_load_op(attach.load_op);
		color_attachment_desc.storeOp = to_vk_store_op(attach.store_op);
		color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment_desc.initialLayout = to_vk_layout[(uint)attach.initial_layout]; //todo not sure this is correct

        VkImageLayout final_layout;
        
		if (attach.num_mips > 1) final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		else final_layout = to_vk_layout[(uint)attach.final_layout];

        if (attach.num_samples == 1) {
            color_attachment_desc.finalLayout = final_layout;
            attachments_desc.append(color_attachment_desc);
        } else {
            VkAttachmentDescription resolve_attachment_desc = color_attachment_desc;
            resolve_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            resolve_attachment_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            resolve_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
            resolve_attachment_desc.finalLayout = final_layout;
            
            color_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color_attachment_desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments_desc.append(color_attachment_desc);
            
            VkAttachmentReference resolve_reference = {};
            resolve_reference.attachment = attachments_desc.length;
            resolve_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            
            resolve_attachment_ref.append(resolve_reference);
            attachments_desc.append(resolve_attachment_desc);
        }
	}

	bool depth_attachment = desc.depth_buffer != DepthBufferFormat::None;
    bool stencil_attachment = desc.stencil_buffer != StencilBufferFormat::None;
	bool store_depth_buffer = desc.depth_attachment;

	VkAttachmentReference depth_attachment_ref = {};

	if (depth_attachment) {
		VkFormat depth_format = find_depth_format(physical_device, stencil_attachment);
        VkSampleCountFlagBits samples = store_depth_buffer ? find_sample_count(max_samples, desc.depth_attachment->num_samples) : attachments_desc[0].samples;

		VkAttachmentDescription depth_attachment_desc = {};
		depth_attachment_desc.format = depth_format;
		depth_attachment_desc.samples = samples;
		depth_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment_desc.storeOp = store_depth_buffer ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment_desc.stencilLoadOp = desc.stencil_buffer == StencilBufferFormat::None ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment_desc.finalLayout = store_depth_buffer ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		//todo VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL

		depth_attachment_ref.attachment = attachments_desc.length;
		depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		attachments_desc.append(depth_attachment_desc);
	}

	for (uint pass = 0; pass < subpasses.length; pass++) {
		SubpassDesc& subpass_desc = subpasses[pass];
		
		tvector<VkAttachmentReference> color_attachment_ref;

		for (uint attachment : subpass_desc.color_attachments) {
			VkAttachmentReference ref = {};
			ref.attachment = attachment;
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			color_attachment_ref.append(ref);
		}

		if (pass == 0) {
			if (depth_attachment) {
				VkSubpassDependency write_depth_dependency = {};
				write_depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
				write_depth_dependency.dstSubpass = 0;
				write_depth_dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				write_depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				write_depth_dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				write_depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				write_depth_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

				dependencies.append(write_depth_dependency);
			}
			if (desc.color_attachments.length > 0) {
				VkSubpassDependency write_color_dependency = {};
				write_color_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
				write_color_dependency.dstSubpass = 0;
				write_color_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				write_color_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				write_color_dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				write_color_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				write_color_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

				dependencies.append(write_color_dependency);
			}

			for (uint i = 0; i < desc.dependency.length; i++) {
				const Dependency& dependency = desc.dependency[i];
				const RenderPassInfo& info = framegraph.info[dependency.id];

				VkPipelineStageFlags src_stage_mask = 0;
				VkPipelineStageFlags dst_stage_mask = 0;
				VkAccessFlags src_access_mask = 0;
				VkAccessFlags dst_access_mask = 0;

                if (dependency.aspect & TextureAspect::Color) {
                    src_stage_mask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    src_access_mask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                }
                if(dependency.aspect & TextureAspect::Depth) {
                    src_stage_mask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    src_access_mask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                }
                if(dependency.stage & VERTEX_STAGE || dependency.stage & FRAGMENT_STAGE) {
                    dst_access_mask |= VK_ACCESS_SHADER_READ_BIT;
                }
                if(dependency.stage & TRANSFER_BIT) {
                    dst_access_mask |= VK_ACCESS_TRANSFER_READ_BIT;
                }
				
				if (dependency.stage & VERTEX_STAGE) dst_stage_mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
				if (dependency.stage & FRAGMENT_STAGE) dst_stage_mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                if (dependency.stage & TRANSFER_BIT) dst_stage_mask |= VK_PIPELINE_STAGE_TRANSFER_BIT;

				VkSubpassDependency subpass_dependency = {};
				subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
				subpass_dependency.dstSubpass = 0;
				subpass_dependency.srcStageMask = src_stage_mask;
				subpass_dependency.dstStageMask = dst_stage_mask;
				subpass_dependency.srcAccessMask = src_access_mask;
				subpass_dependency.dstAccessMask = dst_access_mask;
				subpass_dependency.dependencyFlags = 0;

				dependencies.append(subpass_dependency);
			}
		}
		else {
			//todo somewhat oversimplified


			bool is_color = subpasses[pass - 1].color_attachments.length > 0;

			VkSubpassDependency subpass_dependency = {};
			subpass_dependency.srcSubpass = pass - 1;
			subpass_dependency.dstSubpass = pass;
			subpass_dependency.srcStageMask = is_color ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			subpass_dependency.dstStageMask = is_color ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

			subpass_dependency.srcAccessMask = is_color ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			subpass_dependency.dstAccessMask = is_color ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			subpass_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies.append(subpass_dependency);
		}

		if (subpass_desc.depth_attachment) assert(depth_attachment);

		//todo support for input attachments
		VkSubpassDescription& subpass = subpass_description[pass];
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = color_attachment_ref.length;
		subpass.pColorAttachments = color_attachment_ref.data;
		subpass.pDepthStencilAttachment = subpass_desc.depth_attachment ? &depth_attachment_ref : NULL;
	}
    
    subpass_description[subpasses.length - 1].pResolveAttachments = resolve_attachment_ref.length > 0 ? resolve_attachment_ref.data : nullptr;
    
	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = attachments_desc.length;
	render_pass_info.pAttachments = attachments_desc.data;
	render_pass_info.subpassCount = subpasses.length;
	render_pass_info.pSubpasses = subpass_description;
	render_pass_info.dependencyCount = dependencies.length;
	render_pass_info.pDependencies = dependencies.data;

	VkRenderPass render_pass;
	VK_CHECK(vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass));
	return render_pass;
}

VkFramebuffer make_Framebuffer(VkDevice device, VkPhysicalDevice physical_device, VkRenderPass render_pass, FramebufferDesc& desc, RenderPassInfo& info) {
	array<MAX_ATTACHMENT, VkImageView> attachments_view;
    
    VkSampleCountFlagBits max_samples = get_max_usable_sample_count(physical_device);

	for (uint i = 0; i < desc.color_attachments.length; i++) {
		AttachmentDesc& attach = desc.color_attachments[i];
        bool msaa = attach.num_samples > 1;

        VkSampleCountFlagBits samples = find_sample_count(max_samples, attach.num_samples);
		VkImageUsageFlags usage = to_vk_usage_flags(attach.usage);
		VkFormat format = find_color_format(physical_device, attach); //  VK_FORMAT_R8G8B8_UINT;

		//todo add support for mip-mapping
		VkImageCreateInfo create_info = image_create_default;
		create_info.format = format;
		create_info.mipLevels = msaa ? 1 : attach.num_mips;
		create_info.extent.width = desc.width;
		create_info.extent.height = desc.height;
        create_info.usage = usage;
        create_info.samples = samples;
        
		if (attach.num_mips > 1) create_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		Attachment attachment = {};
		VK_CHECK(vkCreateImage(device, &create_info, nullptr, &attachment.image));
		alloc_and_bind_memory(rhi.texture_allocator, attachment.image); //todo use transient memory for msaa
        
        VkImageViewCreateInfo image_view_info = image_view_create_default;
        image_view_info.image = attachment.image;
        image_view_info.format = format;
        
        VkImageView image_view;
        VK_CHECK(vkCreateImageView(device, &image_view_info, nullptr, &image_view));
				
		attachment.view = image_view;
		attachment.format = format;
        attachment.samples = samples;
		attachment.mips = attach.num_mips;
		attachment.final_layout = to_vk_layout[(int)attach.final_layout];
        attachment.aspect = VK_IMAGE_ASPECT_COLOR_BIT;

		attachments_view.append(image_view);
		info.attachments.append(attachment);
        
        //todo leaking resources
        if (msaa) { //RESOLVE ATTACHMENT
            VkImageCreateInfo create_info_resolved = create_info;
            create_info_resolved.samples = VK_SAMPLE_COUNT_1_BIT;
            create_info_resolved.mipLevels = attach.num_mips;
            
            VkImage resolved_image; //todo store in attachment
            VK_CHECK(vkCreateImage(device, &create_info_resolved, nullptr, &resolved_image));
            alloc_and_bind_memory(rhi.texture_allocator, resolved_image);
            
            image_view_info.image = resolved_image;
            image_view_info.subresourceRange.levelCount = attach.num_mips;
            VK_CHECK(vkCreateImageView(device, &image_view_info, nullptr, &image_view));
            
            attachments_view.append(image_view);
        }
        else if (attachment.mips != 1) {
            image_view_info.subresourceRange.levelCount = create_info.mipLevels;
            
            VK_CHECK(vkCreateImageView(device, &image_view_info, nullptr, &image_view));
        }
		
		{
			Texture texture;
			texture.alloc_info = nullptr;
            texture.desc.usage = attach.usage;
			texture.desc.format = attach.format;
			texture.image = attachment.image;
			texture.desc.width = desc.width;
			texture.desc.height = desc.height;
			texture.desc.num_channels = attach.num_channels;
			texture.desc.num_mips = attach.num_mips;
			texture.desc.format = attach.format;
            texture.desc.aspect = TextureAspect::Color;
            
            texture.view = image_view;
			if(attach.tex_id) *attach.tex_id = assets.textures.assign_handle(std::move(texture));
		}
	}

	if (desc.depth_buffer != DepthBufferFormat::None) {
        bool stencil_attachment = desc.stencil_buffer != StencilBufferFormat::None;
		VkFormat depth_format = find_depth_format(physical_device, stencil_attachment);

		bool store_depth = desc.depth_attachment;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (stencil_attachment) aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        
        VkSampleCountFlagBits samples = !store_depth ? info.attachments[0].samples : find_sample_count(max_samples, desc.depth_attachment->num_samples);

		Attachment attachment = {};
        VkImageCreateInfo create_info = image_create_default;
        create_info.extent.width = desc.width;
        create_info.extent.height = desc.height;
        create_info.format = depth_format;
        create_info.samples = samples;
        create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if(store_depth) create_info.usage |= to_vk_usage_flags(desc.depth_attachment->usage);
        
        VK_CHECK(vkCreateImage(device, &create_info, nullptr, &attachment.image));
        alloc_and_bind_memory(rhi.texture_allocator, attachment.image);
        
		//make_alloc_Image(device, physical_device, desc.width, desc.height, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | (store_depth ? VK_IMAGE_USAGE_SAMPLED_BIT : 0), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &attachment.image, &attachment.memory);
		attachment.view = make_ImageView(device, attachment.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
        attachment.samples = samples;
        attachment.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    
		attachments_view.append(attachment.view);

        info.attachments.append(attachment);
        
		if (store_depth) {
			Texture texture;
			texture.alloc_info = nullptr;
			texture.desc.format = TextureFormat::DEPTH; // todo: specify actual depth formats
			texture.view = attachment.view;
			texture.image = attachment.image;
			texture.desc.width = desc.width;
			texture.desc.height = desc.height;
			texture.desc.num_channels = 1;
            texture.desc.aspect = TextureAspect::Depth;
            if(stencil_attachment) texture.desc.aspect |= TextureAspect::Stencil;
            texture.desc.usage = desc.depth_attachment->usage;

			*desc.depth_attachment->tex_id = assets.textures.assign_handle(std::move(texture));
		}
		else info.transient_attachments.append(attachment);
	}


	//for (Dependency& dependency : desc.dependency) {
	//	RenderPassInfo& info = framegraph.info[dependency.id];
	//	attachments_view.append(info.attachments[0].view);
	//}

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = render_pass;
	framebufferInfo.attachmentCount = attachments_view.length;
	framebufferInfo.pAttachments = attachments_view.data;
	framebufferInfo.width = desc.width;
	framebufferInfo.height = desc.height;
	framebufferInfo.layers = 1;

	VkFramebuffer framebuffer;
	if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
		throw "failed to make framebuffer!";
	}
	return framebuffer;
}
