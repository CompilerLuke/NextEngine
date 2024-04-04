#pragma once

#include "volk.h"
#include "graphics/rhi/frame_buffer.h"
#include "graphics/rhi/vulkan/device.h"

const uint MAX_FRAMEBUFFER = 20;
const uint MAX_ATTACHMENT = 10;
const uint MAX_SUBPASS = 5;

struct Attachment {
	VkDeviceMemory memory;
	VkImage image;
	VkImageView view;
	VkFormat format;
	VkImageLayout final_layout;
    VkSampleCountFlagBits samples;
    VkImageAspectFlagBits aspect;
	uint mips = 1;
};

//todo rename
struct RenderPassInfo {
	uint width;
	uint height;
	array<10, Dependency> dependencies;
	int dependency_chain_length = -1;

	array<MAX_SUBPASS, RenderPass::Type> types;
	array<MAX_ATTACHMENT, Attachment> attachments;
	array<MAX_ATTACHMENT, Attachment> transient_attachments;
};

VkFormat find_depth_format(VkPhysicalDevice physical_device, bool stencil);

RenderPass::ID register_render_pass(VkRenderPass pass);

void submit_framegraph();
void register_wsi_pass(VkRenderPass render_pass, uint width, uint height);
VkRenderPass make_RenderPass(VkDevice device, VkPhysicalDevice physical_device, FramebufferDesc& desc, slice<SubpassDesc> subpass);
VkFramebuffer make_Framebuffer(VkDevice device, VkPhysicalDevice physical_device, VkRenderPass render_pass, FramebufferDesc& desc, struct RenderPassInfo&);
