#include "core/core.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/draw.h"
#include "graphics/rhi/vulkan/swapchain.h"
#include "graphics/rhi/vulkan/frame_buffer.h"
#include "graphics/pass/render_pass.h"
#include "graphics/assets/assets_store.h"

AttachmentDesc& add_color_attachment(FramebufferDesc& desc, texture_handle* handle) {
	AttachmentDesc attachment;
	attachment.tex_id = handle;

	desc.color_attachments.append(attachment);
	return desc.color_attachments.last();
}

AttachmentDesc& add_depth_attachment(FramebufferDesc& desc, texture_handle* handle, DepthBufferFormat format) {
	AttachmentDesc* attachment = TEMPORARY_ALLOC(AttachmentDesc);
	attachment->tex_id = handle;
	desc.depth_buffer = format;

	desc.depth_attachment = attachment;
	return *attachment;
}

void add_dependency(FramebufferDesc& desc, Stage stage, RenderPass::ID id) {
	desc.dependency.append({ stage, id });
}

struct Framegraph {
	uint framebuffer_count = RenderPass::ScenePassCount;

	VkFramebuffer framebuffer[MAX_FRAMEBUFFER]; //[MAX_FRAMES_IN_FLIGHT] , determine wether it's necessary to triple buffer!
	VkEvent render_pass_complete[MAX_FRAMES_IN_FLIGHT][MAX_FRAMEBUFFER];
	VkRenderPass render_pass[MAX_FRAMEBUFFER];
	RenderPassInfo info[MAX_FRAMEBUFFER];
	CommandBuffer* submitted_cmd_buffer[MAX_FRAMEBUFFER];

	array<MAX_FRAMEBUFFER, RenderPass::ID> render_pass_order;
};

static Framegraph framegraph;

render_pass_handle render_pass_by_id(RenderPass::ID pass_id) {
	return { (u64)framegraph.render_pass[pass_id] };
}
 
Viewport render_pass_viewport_by_id(RenderPass::ID pass_id) {
	Viewport viewport = {};
	viewport.width = framegraph.info[pass_id].width;
	viewport.height = framegraph.info[pass_id].height;
	return viewport;
}

const uint MAX_DEP_CHAIN_LENGTH = 5;

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

void make_Framebuffer(RenderPass::ID id, FramebufferDesc& desc) {
	VkDevice device = rhi.device.device;
	VkPhysicalDevice physical_device = rhi.device.physical_device;
	VkRenderPass render_pass = make_RenderPass(device, physical_device, desc);

	framegraph.render_pass[id] = render_pass;
	
	RenderPassInfo& info = framegraph.info[id];

	info.type   = desc.color_attachments.length == 0 ? RenderPass::Depth : RenderPass::Color;
	info.width  = desc.width;
	info.height = desc.height;
	info.dependencies = (slice<Dependency>)desc.dependency;

	framegraph.framebuffer[id] = make_Framebuffer(device, physical_device, render_pass, desc, info);

	for (uint frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
		framegraph.render_pass_complete[frame][id] = make_Event(device);
	}
}

void make_wsi_pass(slice<Dependency> dependency) {
	framegraph.info[RenderPass::Screen].dependencies = dependency;
	framegraph.info[RenderPass::Screen].type = RenderPass::Color;
	//todo add support for dependencies

}

void register_wsi_pass(VkRenderPass render_pass, uint width, uint height) {
	framegraph.info[RenderPass::Screen].type = RenderPass::Color;
	framegraph.info[RenderPass::Screen].width = width;
	framegraph.info[RenderPass::Screen].height = height;
	framegraph.render_pass[RenderPass::Screen] = render_pass;
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
	VkClearValue clear_colors[2] = {};
	clear_colors[0].color = { clear_color.x, clear_color.y, clear_color.z, clear_color.w };
	clear_colors[1].depthStencil = { 1, 0 };

	render_pass_info.clearValueCount = 2;
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



	return { id, view.type, {(u64)vk_render_pass}, viewport, cmd_buffer };
}

void generate_mips_after_render_pass(VkCommandBuffer cmd_buffer, RenderPassInfo& info, Attachment& attachment) {
	int mip_width = info.width;
	int mip_height = info.height;

	VkImageLayout final_layout = attachment.final_layout; 

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = attachment.image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, 0, 1, &barrier);

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
	vkCmdEndRenderPass(render_pass.cmd_buffer);

	VkEvent event = framegraph.render_pass_complete[rhi.frame_index][render_pass.id];
	if (event) vkCmdSetEvent(render_pass.cmd_buffer, event, render_pass.type == RenderPass::Color ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

	{
		RenderPassInfo& info = framegraph.info[render_pass.id];
		Attachment& attachment = info.attachments[0];

		if (attachment.mips > 1) generate_mips_after_render_pass(render_pass.cmd_buffer, info, attachment);
	}

	assert(render_pass.cmd_buffer.cmd_buffer != VK_NULL_HANDLE);
	framegraph.submitted_cmd_buffer[render_pass.id] = &render_pass.cmd_buffer;

	//end_draw_cmds(render_pass.cmd_buffer);
}

void submit_framegraph(QueueSubmitInfo& submit_info) {
	for (RenderPass::ID id : framegraph.render_pass_order) {
		if (!framegraph.submitted_cmd_buffer[id]) continue;

		end_draw_cmds(*framegraph.submitted_cmd_buffer[id]);
		
		
		framegraph.submitted_cmd_buffer[id] = nullptr;

		//assert(cmd_buffer.cmd_buffer, "Never completed render pass!");


		//VkPipelineStageFlags src_stage_flags = 0;
		//VkPipelineStageFlags dst_stage_flags = 0;
		//array<10, VkImageMemoryBarrier> image_barriers;
		/*
				CommandBuffer& cmd_buffer = *framegraph.submitted_cmd_buffer[id];

		for (const Dependency& dependency : framegraph.view[id].dependencies) {
			const RenderPassInfo& info = framegraph.view[dependency.id];

			//todo be more fine grained in the future
			//also optimize using pipeline barrier instead of event, if the two passes are directly after each other
			
			VkPipelineStageFlags src_stage_flags = 0;
			VkPipelineStageFlags dst_stage_flags = 0;
			
			src_stage_flags |= info.type == RenderPass::Color ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			if (dependency.stage & VERTEX_STAGE) dst_stage_flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
			if (dependency.stage & FRAGMENT_STAGE) dst_stage_flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			bool is_color = info.type == RenderPass::Color;

			//todo image barrier for each attachment!
			VkImageMemoryBarrier image_barrier = {};
			image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			image_barrier.oldLayout = is_color ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			image_barrier.image = info.image_attachments[0];
			image_barrier.subresourceRange.baseMipLevel = 0;
			image_barrier.subresourceRange.levelCount = 1;
			image_barrier.subresourceRange.baseArrayLayer = 0;
			image_barrier.subresourceRange.layerCount = 1;
			image_barrier.subresourceRange.aspectMask = is_color ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
			image_barrier.srcAccessMask = is_color ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			image_barrier.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;


			vkCmdWaitEvents(cmd_buffer, 1, &framegraph.render_pass_complete[rhi.frame_index][dependency.id], src_stage_flags, dst_stage_flags, 0, nullptr, 0, nullptr, 1, &image_barrier);
		}
		*/

	}	
}

void end_render_frame(RenderPass& render_pass) {
	CommandPool& graphics_cmd_pool = render_thread.command_pool;
	Swapchain& swapchain = rhi.swapchain;

	vkCmdEndRenderPass(render_pass.cmd_buffer);
	//end_draw_cmds(render_pass.cmd_buffer);
	framegraph.submitted_cmd_buffer[RenderPass::Screen] = &render_pass.cmd_buffer;

	uint current_frame = rhi.frame_index;

	QueueSubmitInfo submit_info = {};
	submit_info.completion_fence = swapchain.in_flight_fences[current_frame];


	//todo HACK!, I need the wait on transfer to occur before everything else!
	//std::swap(graphics_cmd_pool.submited[rhi.frame_index][0], graphics_cmd_pool.submited[rhi.frame_index].last());

	submit_framegraph(submit_info);
	submit_all_cmds(graphics_cmd_pool, submit_info);

	transfer_queue_dependencies(rhi.staging_queue, submit_info, rhi.waiting_on_transfer_frame);
	queue_wait_semaphore(submit_info, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, swapchain.image_available_semaphore[current_frame]);
	queue_signal_semaphore(submit_info, swapchain.render_finished_semaphore[current_frame]);

	vkResetFences(rhi.device, 1, &swapchain.in_flight_fences[current_frame]);

	queue_submit(rhi.device, Queue_Graphics, submit_info);

	present_swapchain_image(rhi.swapchain);

	swapchain.current_frame = (swapchain.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
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

VkFormat find_color_format(VkPhysicalDevice device, AttachmentDesc& desc) {
	return to_vk_image_format(desc.format, desc.num_channels);
}

VkRenderPass make_RenderPass(VkDevice device, VkPhysicalDevice physical_device, FramebufferDesc& desc) {
	array<MAX_ATTACHMENT, VkAttachmentDescription> attachments_desc;
	array<MAX_ATTACHMENT, VkAttachmentReference> color_attachment_ref;

	for (uint i = 0; i < desc.color_attachments.length; i++) {
		AttachmentDesc& attach = desc.color_attachments[i];

		VkFormat format = find_color_format(physical_device, attach);

		VkAttachmentReference ref = {};
		ref.attachment = attachments_desc.length;
		ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription color_attachment_desc = {};
		color_attachment_desc.format = format;
		color_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment_desc.initialLayout = to_vk_layout[(uint)attach.initial_layout]; //todo not sure this is correct
		
		if (attach.num_mips > 1) color_attachment_desc.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		else color_attachment_desc.finalLayout = to_vk_layout[(uint)attach.final_layout];

		attachments_desc.append(color_attachment_desc);
		color_attachment_ref.append(ref);
	}


	VkAttachmentReference depth_attachment_ref = {};

	bool depth_attachment = desc.depth_buffer != Disable_Depth_Buffer;

	if (depth_attachment) {
		VkFormat depth_format = find_depth_format(physical_device);

		bool store_depth_buffer = desc.depth_attachment;
		depth_attachment_ref.attachment = attachments_desc.length;
		depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depth_attachment_desc = {};
		depth_attachment_desc.format = depth_format;
		depth_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment_desc.storeOp = store_depth_buffer ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment_desc.stencilLoadOp = desc.stencil_buffer == Disable_Stencil_Buffer ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment_desc.finalLayout = store_depth_buffer ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL

		attachments_desc.append(depth_attachment_desc);
	}

	VkAttachmentReference input_attachment_ref[10] = {};
	VkSubpassDependency dependencies[10] = {};

	for (uint i = 0; i < desc.dependency.length; i++) {
		Dependency dependency = desc.dependency[i];
		const RenderPassInfo& info = framegraph.info[dependency.id];
		bool is_color = info.type == RenderPass::Color;

		VkPipelineStageFlags src_stage_mask = 0;
		VkPipelineStageFlags dst_stage_mask = 0;

		src_stage_mask |= is_color ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		if (dependency.stage & VERTEX_STAGE) dst_stage_mask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		if (dependency.stage & FRAGMENT_STAGE) dst_stage_mask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

		VkSubpassDependency& subpass_dependency = dependencies[i];
		subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpass_dependency.dstSubpass = 0;
		subpass_dependency.srcStageMask = src_stage_mask;
		subpass_dependency.dstStageMask = dst_stage_mask;

		//todo add access mask!

		/*
		VkAttachmentReference& ref = input_attachment_ref[i];
		ref.attachment = attachments_desc.length;
		ref.layout = is_color ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		AttachmentDesc dummy = {};
		
		VkAttachmentDescription desc = {};
		desc.initialLayout = ref.layout;
		desc.finalLayout = ref.layout;
		desc.format = is_color ? find_color_format(physical_device, dummy) : find_depth_format(physical_device);
		desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		
		attachments_desc.append(desc);
		*/
	}

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = color_attachment_ref.length;
	subpass.pColorAttachments = color_attachment_ref.data;
	subpass.pDepthStencilAttachment = depth_attachment ? &depth_attachment_ref : NULL;
	//subpass.inputAttachmentCount = desc.dependency.length;
	//subpass.pInputAttachments = input_attachment_ref;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = attachments_desc.length;
	render_pass_info.pAttachments = attachments_desc.data;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = desc.dependency.length;
	render_pass_info.pDependencies = dependencies;

	VkRenderPass render_pass;
	VK_CHECK(vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass));
	return render_pass;
}

VkFramebuffer make_Framebuffer(VkDevice device, VkPhysicalDevice physical_device, VkRenderPass render_pass, FramebufferDesc& desc, RenderPassInfo& info) {
	array<MAX_ATTACHMENT, VkImageView> attachments_view;

	for (uint i = 0; i < desc.color_attachments.length; i++) {
		AttachmentDesc& attach = desc.color_attachments[i];

		VkImageUsageFlags usage = to_vk_usage_flags(attach.usage);
		VkFormat format = find_color_format(physical_device, attach); //  VK_FORMAT_R8G8B8_UINT;


		//todo add support for mip-mapping
		VkImageCreateInfo create_info = image_create_default;
		create_info.format = format;
		create_info.mipLevels = attach.num_mips;
		create_info.extent.width = desc.width;
		create_info.extent.height = desc.height;
		create_info.usage = to_vk_usage_flags(attach.usage);
		
		if (attach.num_mips > 1) create_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		Attachment attachment = {};
		VK_CHECK(vkCreateImage(device, &create_info, nullptr, &attachment.image));
		alloc_and_bind_memory(rhi.texture_allocator, attachment.image);
				
		attachment.view = make_ImageView(device, attachment.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
		attachment.format = format;
		attachment.mips = attach.num_mips;
		attachment.final_layout = to_vk_layout[(int)attach.final_layout];

		attachments_view.append(attachment.view);

		info.attachments.append(attachment);
		
		{
			VkImageViewCreateInfo info = image_view_create_default;
			info.image = attachment.image;
			info.format = format;
			info.subresourceRange.levelCount = attach.num_mips;

			Texture texture;
			texture.alloc_info = nullptr;
			texture.desc.format = attach.format;
			texture.image = attachment.image;
			texture.desc.width = desc.width;
			texture.desc.height = desc.height;
			texture.desc.num_channels = attach.num_channels;
			texture.desc.num_mips = attach.num_mips;
			texture.desc.format = attach.format;

			VK_CHECK(vkCreateImageView(device, &info, nullptr, &texture.view));

			*attach.tex_id = assets.textures.assign_handle(std::move(texture));
		}
	}

	if (desc.depth_buffer != Disable_Depth_Buffer) {
		VkFormat depth_format = find_depth_format(physical_device);

		bool store_depth = desc.depth_attachment;

		Attachment attachment = {};
		make_alloc_Image(device, physical_device, desc.width, desc.height, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | (store_depth ? VK_IMAGE_USAGE_SAMPLED_BIT : 0), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &attachment.image, &attachment.memory);
		attachment.view = make_ImageView(device, attachment.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

		attachments_view.append(attachment.view);

		if (store_depth) {
			info.attachments.append(attachment);

			Texture texture;
			texture.alloc_info = nullptr;
			//texture.desc.format = depth_format;
			texture.view = attachment.view;
			texture.image = attachment.image;
			texture.desc.width = desc.width;
			texture.desc.height = desc.height;
			texture.desc.num_channels = 1;
			//todo set format

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