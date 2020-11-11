#pragma once

#include "volk.h"
#include "graphics/rhi/pipeline.h"
#include "core/container/hash_map.h"

struct VkPipelineDesc {
	//Required
	VkShaderModule vert_shader;
	VkShaderModule frag_shader;
	VkRenderPass render_pass;
	VkExtent2D extent;
	slice<VkVertexInputBindingDescription> binding_descriptions;
	slice<VkVertexInputAttributeDescription> attribute_descriptions;
	slice<VkDescriptorSetLayout> descriptor_layouts;
	slice<VkPushConstantRange> push_constant_range;

	//Settings
	VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
	float line_width = 1.0f;
	VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
	VkColorComponentFlags color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	bool depth_test_enable = VK_TRUE;
	bool depth_write_enable = VK_TRUE;
	bool stencil_test_enable = VK_FALSE;
	bool alpha_blend_enable = VK_FALSE;
    VkStencilOpState front_stencil = {};
	VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS;
	VkBlendOp blend_op;
	VkBlendFactor src_blend_factor;
	VkBlendFactor dst_blend_factor;
    VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
    
	uint subpass = 0;
};

void make_GraphicsPipeline(VkDevice device, VkPipelineDesc& desc, VkPipelineLayout* pipeline_layout, VkPipeline* pipeline);

#define MAX_PIPELINE 271

struct PipelineCache {
	hash_set<PipelineDesc, MAX_PIPELINE> keys;
	VkPipeline pipelines[MAX_PIPELINE];
	VkPipelineLayout layouts[MAX_PIPELINE];
};

pipeline_handle query_Pipeline(PipelineCache&, const PipelineDesc& desc);
VkPipeline get_Pipeline(PipelineCache&, pipeline_handle handle);
VkPipelineLayout get_pipeline_layout(PipelineCache&, pipeline_handle handle);
VkPipelineLayout get_pipeline_layout(PipelineCache&, pipeline_layout_handle handle);

const VkAttachmentDescription default_color_attachment = {
	0,
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_SAMPLE_COUNT_1_BIT,
	VK_ATTACHMENT_LOAD_OP_CLEAR,
	VK_ATTACHMENT_STORE_OP_STORE,
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
	VK_IMAGE_LAYOUT_UNDEFINED,
	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
};

const VkAttachmentDescription default_depth_attachment = {
	0,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_SAMPLE_COUNT_1_BIT,
	VK_ATTACHMENT_LOAD_OP_CLEAR,
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_STORE_OP_DONT_CARE,
	VK_IMAGE_LAYOUT_UNDEFINED,
	VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
};
