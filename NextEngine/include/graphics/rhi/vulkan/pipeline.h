#pragma once

#include "volk.h"

struct PipelineDesc {
	//Required
	VkShaderModule vert_shader;
	VkShaderModule frag_shader;
	VkRenderPass render_pass;
	VkExtent2D extent;
	slice<VkVertexInputBindingDescription> binding_descriptions;
	slice<VkVertexInputAttributeDescription> attribute_descriptions;
	slice<VkDescriptorSetLayout> descriptor_layouts;

	//Settings
	VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
	float line_width = 1.0f;
	VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
	VkColorComponentFlags color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	bool depth_test_enable = VK_TRUE;
	bool depth_write_enable = VK_TRUE;
	bool stencil_test_enable = VK_FALSE;
	VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS;
};

void make_GraphicsPipeline(VkDevice device, PipelineDesc& desc, VkPipelineLayout* pipeline_layout, VkPipeline* pipeline);