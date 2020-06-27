#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/pipeline.h"
#include "graphics/rhi/vulkan/shader.h"
#include "graphics/assets/shader.h"
#include "core/container/string_buffer.h"
#include "core/io/vfs.h"
#include "core/container/slice.h"
#include "core/memory/linear_allocator.h"
#include "core/container/hash_map.h"


VkPolygonMode vk_polygon_mode(DrawCommandState state) {
	VkPolygonMode polygon_modes[3] = { VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_LINE };
	return polygon_modes[decode_DrawState(PolyMode_Offset, state, 2)];
}

uint vk_line_width(DrawCommandState state) {
	return decode_DrawState(WireframeLineWidth_Offset, state, 8);
}

VkCullModeFlags vk_cull_mode(DrawCommandState state) {
	VkCullModeFlags cull_modes[3] = { VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_BACK_BIT, VK_CULL_MODE_NONE };
	return cull_modes[decode_DrawState(Cull_Offset, state, 2)];
}

VkCompareOp vk_depth_compare_op(DrawCommandState state) {
	VkCompareOp depth_funcs[3] = { VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_NEVER };
	return depth_funcs[decode_DrawState(DepthFunc_Offset, state, 2)];
}

VkColorComponentFlags vk_color_mask(DrawCommandState state) {
	VkColorComponentFlags color_masks[2] = { VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, 0 };
	return color_masks[decode_DrawState(ColorMask_Offset, state, 2)];
}

bool vk_enable_alpha_blend(DrawCommandState state) {
	bool enable[] = {false, true};
	return enable[decode_DrawState(BlendMode_Offset, state, 2)];
}

VkBlendOp vk_alpha_blend_op(DrawCommandState state) {
	array<2, VkBlendOp> ops = { VK_BLEND_OP_ADD, VK_BLEND_OP_ADD };
	return ops[decode_DrawState(BlendMode_Offset, state, 2)];
}

VkBlendFactor vk_src_alpha_blend_factor(DrawCommandState state) {
	VkBlendFactor factor[] = { VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_ALPHA };
	return factor[decode_DrawState(BlendMode_Offset, state, 2)];
}

VkBlendFactor vk_dst_alpha_blend_factor(DrawCommandState state) {
	VkBlendFactor factor[] = { VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA };
	return factor[decode_DrawState(BlendMode_Offset, state, 2)];
}

bool vk_depth_write(DrawCommandState state) {
	bool depth_write[2] = {true, false};
	return depth_write[decode_DrawState(DepthMask_Offset, state, 1)];
}

bool vk_depth_test(DrawCommandState state) {
	bool depth_test[3] = { true, true, false };
	return depth_test[decode_DrawState(DepthFunc_Offset, state, 2)];
}



VkStencilOpState vk_front_stencil_op(DrawCommandState state) {
	VkCompareOp stencil_test[4] = { VK_COMPARE_OP_LESS, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_NEVER };
	
	VkStencilOpState stencil_state = {};
	stencil_state.compareOp = stencil_test[decode_DrawState(StencilFunc_Offset, state, 4)];
	stencil_state.failOp = VK_STENCIL_OP_KEEP;
	stencil_state.passOp = VK_STENCIL_OP_REPLACE;
	stencil_state.depthFailOp = VK_STENCIL_OP_KEEP;
	stencil_state.reference = 1;
	stencil_state.compareMask = 0xFF;
	stencil_state.writeMask = decode_DrawState(StencilMask_Offset, state, 8);

	return stencil_state;
}

//todo cache and merge with bottom section

pipeline_layout_handle query_Layout(slice<descriptor_set_handle> descriptors) {
	VkDescriptorSetLayout layouts[10] = {};
	for (uint i = 0; i < descriptors.length; i++) {
		layouts[i] = get_descriptor_set_layout(descriptors[i]);
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = descriptors.length;
	pipelineLayoutInfo.pSetLayouts = layouts;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	VkPipelineLayout pipeline_layout;
	if (vkCreatePipelineLayout(rhi.device, &pipelineLayoutInfo, nullptr, &pipeline_layout) != VK_SUCCESS) {
		throw "failed to make pipeline!";
	}

	return { (u64)pipeline_layout };
}

VkPipelineLayout get_pipeline_layout(PipelineCache& cache, pipeline_layout_handle handle) {
	return (VkPipelineLayout)handle.id;
}

//todo generate graphics pipeline directly from PipelineDesc, instead of first converting to VkPipelineDesc
void make_GraphicsPipeline(VkDevice device, VkPipelineDesc& desc, VkPipelineLayout* pipeline_layout, VkPipeline* pipeline) {
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = desc.vert_shader;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = desc.frag_shader;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	vertexInputInfo.vertexBindingDescriptionCount = desc.binding_descriptions.length;
	vertexInputInfo.pVertexBindingDescriptions = desc.binding_descriptions.data;

	vertexInputInfo.vertexAttributeDescriptionCount = desc.attribute_descriptions.length;
	vertexInputInfo.pVertexAttributeDescriptions = desc.attribute_descriptions.data;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = desc.extent.width;
	viewport.height = desc.extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = desc.extent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = desc.polygon_mode;
	rasterizer.lineWidth = desc.line_width;
	rasterizer.cullMode = desc.cull_mode; // VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = desc.color_write_mask;
	colorBlendAttachment.blendEnable = desc.alpha_blend_enable;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;   //VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; //VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; //VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; //desc.src_blend_factor;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; //desc.dst_blend_factor;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; //desc.blend_op;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	//colorBlending.blendConstants[0] = 1.0f;
	//colorBlending.blendConstants[1] = 1.0f;
	//colorBlending.blendConstants[2] = 1.0f;
	//colorBlending.blendConstants[3] = 1.0f;

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = desc.depth_test_enable; // VK_TRUE;
	depthStencil.depthWriteEnable = desc.depth_write_enable;
	depthStencil.depthCompareOp = desc.depth_compare_op;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	depthStencil.stencilTestEnable = desc.stencil_test_enable;
	depthStencil.front = {};
	depthStencil.back = {};

	//todo add ability to choose!
	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 3;
	dynamicState.pDynamicStates = dynamicStates;


	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = desc.descriptor_layouts.length;
	pipelineLayoutInfo.pSetLayouts = desc.descriptor_layouts.data;
	pipelineLayoutInfo.pushConstantRangeCount = desc.push_constant_range.length;
	pipelineLayoutInfo.pPushConstantRanges = desc.push_constant_range.data;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, pipeline_layout) != VK_SUCCESS) {
		throw "failed to make pipeline!";
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = *pipeline_layout;
	pipelineInfo.renderPass = desc.render_pass; 
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, pipeline) != VK_SUCCESS) {
		throw "failed to make graphics pipeline!";
	}
}

//todo implement actual hash function 
u64 hash_func(PipelineDesc& pipeline_desc) {
	return (pipeline_desc.vertex_layout << 0)
		| (pipeline_desc.instance_layout << 4)
		| (pipeline_desc.render_pass.id << 8)
		| (pipeline_desc.state << 16);
}

//static hash_map<PipelineDesc, VkPipeline, MAX_PIPELINE> pipelines;

VkPipeline get_Pipeline(PipelineCache& cache, pipeline_handle handle) {
	return cache.pipelines[handle.id - 1];
}

VkPipelineLayout get_pipeline_layout(PipelineCache& cache, pipeline_handle handle) {
	return cache.layouts[handle.id - 1];
}

pipeline_handle make_Pipeline(PipelineCache& cache, const PipelineDesc& desc) {
	//todo get actuall shader config

	ENGINE_API Viewport render_pass_viewport_by_id(RenderPass::ID id);

	Viewport viewport{}; // = render_pass_viewport_by_id(desc.render_pass);
	VkRenderPass render_pass = (VkRenderPass)desc.render_pass.id;
	ShaderModules* shader_module = get_shader_config(desc.shader, SHADER_INSTANCED);

	//DESCRIBE PIPELINE
	VkPipelineDesc pipeline_desc = {};
	pipeline_desc.vert_shader = shader_module->vert;
	pipeline_desc.frag_shader = shader_module->frag;
	pipeline_desc.render_pass = render_pass;
	pipeline_desc.extent = { viewport.width, viewport.height };
	pipeline_desc.binding_descriptions = input_bindings(rhi.vertex_layouts, desc.vertex_layout, desc.instance_layout);
	pipeline_desc.attribute_descriptions = input_attributes(rhi.vertex_layouts, desc.vertex_layout, desc.instance_layout);
	pipeline_desc.descriptor_layouts = shader_module->set_layouts; 
	
	pipeline_desc.polygon_mode = vk_polygon_mode(desc.state);
	pipeline_desc.line_width = vk_line_width(desc.state);
	pipeline_desc.cull_mode = vk_cull_mode(desc.state);
	pipeline_desc.depth_test_enable = vk_depth_test(desc.state);
	pipeline_desc.depth_write_enable = vk_depth_write(desc.state);
	pipeline_desc.depth_compare_op = vk_depth_compare_op(desc.state);
	
	pipeline_desc.alpha_blend_enable = vk_enable_alpha_blend(desc.state);
	pipeline_desc.src_blend_factor = vk_src_alpha_blend_factor(desc.state);
	pipeline_desc.dst_blend_factor = vk_dst_alpha_blend_factor(desc.state);
	pipeline_desc.blend_op = vk_alpha_blend_op(desc.state);

	array<2, VkPushConstantRange> ranges = {};

	if (desc.range[0].size > 0) {
		ranges.append({ VK_SHADER_STAGE_VERTEX_BIT, desc.range[0].offset, desc.range[0].size });
	}

	if (desc.range[1].size > 0) {
		ranges.append({ VK_SHADER_STAGE_FRAGMENT_BIT, desc.range[1].offset, desc.range[1].size });
	}

	//todo find strange bug!
	printf("BINDING DESCRIPTIONS length %i\n", pipeline_desc.binding_descriptions[0].binding);

	pipeline_desc.push_constant_range = ranges;

	//todo add support for stencil

	//CREATE PIPELINE
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;

	make_GraphicsPipeline(rhi.device, pipeline_desc, &pipeline_layout, &pipeline);

	//ADD PIPELINE TO CACHE
	uint index = cache.keys.add(desc);
	cache.pipelines[index] = pipeline;
	cache.layouts[index] = pipeline_layout;
	
	return { index + 1 };
}

pipeline_handle query_Pipeline(PipelineCache& cache, const PipelineDesc& desc) {
	int index = cache.keys.index(desc);
	if (index == -1) return make_Pipeline(cache, desc);

	return { (uint)index + 1 };
}

//GLOBAL API

VkPipeline get_Pipeline(pipeline_handle handle) {
	return get_Pipeline(rhi.pipeline_cache, handle);
}

pipeline_handle make_Pipeline(PipelineDesc& desc) {
	return make_Pipeline(rhi.pipeline_cache, desc);
}


pipeline_handle query_Pipeline(const PipelineDesc& desc) {
	return query_Pipeline(rhi.pipeline_cache, desc);
}
