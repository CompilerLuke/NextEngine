#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/pipeline.h"
#include "graphics/rhi/vulkan/shader.h"
#include "graphics/assets/shader.h"
#include "core/container/string_buffer.h"
#include "engine/vfs.h"
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
	VkCompareOp depth_funcs[4] = { VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_ALWAYS };
	return depth_funcs[decode_DrawState(DepthFunc_Offset, state, 2)];
}

VkColorComponentFlags vk_color_mask(DrawCommandState state) {
	VkColorComponentFlags color_masks[2] = { VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, 0 };
	
    return color_masks[decode_DrawState(ColorMask_Offset, state, 1)];
}

bool vk_alpha_blend(DrawCommandState state) {
    bool alpha_blend[2] = {false, true};
	return alpha_blend[decode_DrawState(BlendMode_Offset, state, 2)];
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

VkPrimitiveTopology vk_primitive_topology(DrawCommandState state) {
	VkPrimitiveTopology topology[] = {
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP
	};
	VkPrimitiveTopology result = topology[decode_DrawState(PrimitiveType_Offset, state, 3)];
	assert(result == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || result == VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	return result;
}

bool vk_depth_write(DrawCommandState state) {
	bool depth_write[2] = {true, false};
	return depth_write[decode_DrawState(DepthMask_Offset, state, 1)];
}

bool vk_depth_test(DrawCommandState state) {
	bool depth_test[4] = { true, true, false, true };
	return depth_test[decode_DrawState(DepthFunc_Offset, state, 2)];
}

bool vk_stencil_test(DrawCommandState state) {
    bool stencil_test[4] = { false, true, true, true };
    return stencil_test[decode_DrawState(StencilFunc_Offset, state, 2)];
}

VkStencilOpState vk_front_stencil_op(DrawCommandState state) {
	VkCompareOp stencil_test[4] = { VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_ALWAYS };
	
	VkStencilOpState stencil_state = {};
	stencil_state.compareOp = stencil_test[decode_DrawState(StencilFunc_Offset, state, 4)];
	stencil_state.failOp = VK_STENCIL_OP_KEEP;
	stencil_state.passOp = VK_STENCIL_OP_REPLACE;
	stencil_state.depthFailOp = VK_STENCIL_OP_KEEP;
	stencil_state.reference = 1;
	stencil_state.compareMask =  0xFF;
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
	VkPhysicalDeviceFeatures& device_features = rhi.device.device_features; //todo take in as parameter
	
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
	inputAssembly.topology = desc.topology;
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
    rasterizer.lineWidth = device_features.wideLines ? desc.line_width : 1.0; 
	rasterizer.cullMode = desc.cull_mode;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = desc.depth_bias;
	rasterizer.depthBiasConstantFactor = 0;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = desc.sample_count;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;


	VkPipelineColorBlendAttachmentState colorBlendAttachment[MAX_ATTACHMENTS] = {};
	for (uint i = 0; i < desc.color_attachments; i++) {
		colorBlendAttachment[i].colorWriteMask = desc.color_write_mask;
		colorBlendAttachment[i].blendEnable = desc.alpha_blend_enable;
		colorBlendAttachment[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment[i].colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; //desc.src_blend_factor;
		colorBlendAttachment[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; //desc.dst_blend_factor;
		colorBlendAttachment[i].alphaBlendOp = VK_BLEND_OP_ADD; //desc.blend_op;
	}

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = desc.color_attachments;
	colorBlending.pAttachments = colorBlendAttachment;
	colorBlending.blendConstants[0] = 1.0f;
	colorBlending.blendConstants[1] = 1.0f;
	colorBlending.blendConstants[2] = 1.0f;
	colorBlending.blendConstants[3] = 1.0f;

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = desc.depth_test_enable; 
	depthStencil.depthWriteEnable = desc.depth_write_enable;
	depthStencil.depthCompareOp = desc.depth_compare_op;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	depthStencil.stencilTestEnable = desc.stencil_test_enable;
	depthStencil.front = desc.front_stencil;
    depthStencil.back = desc.front_stencil;

	//todo add ability to choose!
	array<5, VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
        //VK_DYNAMIC_STATE_LINE_WIDTH,
	};

	if (desc.depth_bias) {
		dynamicStates.append(VK_DYNAMIC_STATE_DEPTH_BIAS);
	}

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = dynamicStates.length;
	dynamicState.pDynamicStates = dynamicStates.data;


	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = desc.descriptor_layouts.length;
	pipelineLayoutInfo.pSetLayouts = desc.descriptor_layouts.data;
	pipelineLayoutInfo.pushConstantRangeCount = desc.push_constant_range.length;
	pipelineLayoutInfo.pPushConstantRanges = desc.push_constant_range.data;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, pipeline_layout) != VK_SUCCESS) {
		fprintf(stderr, "failed to make pipeline layout!\n");
        abort();
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
	pipelineInfo.subpass = desc.subpass;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;
    

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, pipeline) != VK_SUCCESS) {
        /*for (VkVertexInputAttributeDescription& attrib : desc.attribute_descriptions) {
            printf("Attribute (%i), format: %i, binding: %i, offset: %i\n", attrib.location, attrib.format, attrib.binding, attrib.offset);
        }
        
        fprintf(stderr, "Failed to make graphics pipeline!\n");
        fprintf(stderr, "Num vert attributes %i\n", vertexInputInfo.vertexAttributeDescriptionCount);
        fprintf(stderr, "Num vert bindings %i\n", vertexInputInfo.vertexBindingDescriptionCount);
        */
        
        abort();
	}
}

//todo implement actual hash function 
u64 hash_func(GraphicsPipelineDesc& pipeline_desc) {
	return (pipeline_desc.vertex_layout << 0)
		| (pipeline_desc.instance_layout << 4)
		| (pipeline_desc.render_pass << 8)
		| (pipeline_desc.state << 16);
}

//static hash_map<PipelineDesc, VkPipeline, MAX_PIPELINE> pipelines;

VkPipeline get_Pipeline(PipelineCache& cache, pipeline_handle handle) {
	return cache.pipelines[handle.id - 1];
}

VkPipelineLayout get_pipeline_layout(PipelineCache& cache, pipeline_handle handle) {
	return cache.layouts[handle.id - 1];
}

pipeline_handle make_Pipeline(PipelineCache& cache, const GraphicsPipelineDesc& desc) {
	//todo get actuall shader config

	//ENGINE_API Viewport render_pass_viewport_by_id(RenderPass::ID id);

	Viewport viewport{}; // = render_pass_viewport_by_id(desc.render_pass);
    VkRenderPass render_pass;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDesc pipeline_desc = {};
    
    if (desc.render_pass > 10000) { //todo find alternative flagging method
        render_pass = (VkRenderPass)(desc.render_pass);
    } else {
        render_pass = (VkRenderPass)render_pass_by_id((RenderPass::ID)desc.render_pass).id;
        samples = (VkSampleCountFlagBits)render_pass_samples_by_id((RenderPass::ID)desc.render_pass);
		pipeline_desc.color_attachments = render_pass_num_color_attachments_by_id((RenderPass::ID)desc.render_pass, desc.subpass);
	}
    
	ShaderModules* shader_module = get_shader_config(desc.shader, desc.shader_flags);

	//DESCRIBE PIPELINE
    auto binding_descriptions = input_bindings(rhi.vertex_layouts, desc.vertex_layout, desc.instance_layout);
    auto attribute_descriptions = input_attributes(rhi.vertex_layouts, desc.vertex_layout, desc.instance_layout);
    
	pipeline_desc.subpass = desc.subpass;
	pipeline_desc.vert_shader = shader_module->vert;
	pipeline_desc.frag_shader = shader_module->frag;
	pipeline_desc.render_pass = render_pass;
	pipeline_desc.extent = { viewport.width, viewport.height };
	pipeline_desc.binding_descriptions = binding_descriptions;
	pipeline_desc.attribute_descriptions = attribute_descriptions;
	pipeline_desc.descriptor_layouts = shader_module->set_layouts; 
	
	pipeline_desc.polygon_mode = vk_polygon_mode(desc.state);
	pipeline_desc.topology = vk_primitive_topology(desc.state);
	pipeline_desc.color_write_mask = vk_color_mask(desc.state);
	pipeline_desc.line_width = vk_line_width(desc.state);
	pipeline_desc.cull_mode = vk_cull_mode(desc.state);
	pipeline_desc.depth_test_enable = vk_depth_test(desc.state);
	pipeline_desc.depth_write_enable = vk_depth_write(desc.state);
	pipeline_desc.depth_compare_op = vk_depth_compare_op(desc.state);
	

	pipeline_desc.alpha_blend_enable = vk_alpha_blend(desc.state);
	pipeline_desc.src_blend_factor = vk_src_alpha_blend_factor(desc.state);
	pipeline_desc.dst_blend_factor = vk_dst_alpha_blend_factor(desc.state);
	pipeline_desc.blend_op = vk_alpha_blend_op(desc.state);
    pipeline_desc.stencil_test_enable = vk_stencil_test(desc.state);
    pipeline_desc.front_stencil = vk_front_stencil_op(desc.state);
    pipeline_desc.sample_count = samples;
	pipeline_desc.depth_bias = desc.state & DynamicState_DepthBias;

	array<2, VkPushConstantRange> ranges = {};

	if (desc.range[0].size > 0) {
		ranges.append({ VK_SHADER_STAGE_VERTEX_BIT, desc.range[0].offset, desc.range[0].size });
	}

	if (desc.range[1].size > 0) {
		ranges.append({ VK_SHADER_STAGE_FRAGMENT_BIT, desc.range[1].offset, desc.range[1].size });
	}

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

pipeline_handle query_Pipeline(PipelineCache& cache, const GraphicsPipelineDesc& desc) {
	int index = cache.keys.index(desc);
	if (index == -1) {
		static uint count = 0;
		printf("BINDING DESCRIPTORS #%i\n", count++);
		return make_Pipeline(cache, desc);
	}

	return { (uint)index + 1 };
}

//GLOBAL API

VkPipeline get_Pipeline(pipeline_handle handle) {
	return get_Pipeline(rhi.pipeline_cache, handle);
}

pipeline_handle query_Pipeline(const GraphicsPipelineDesc& desc) {
	return query_Pipeline(rhi.pipeline_cache, desc);
}

void reload_Pipeline(const GraphicsPipelineDesc& desc) {
	make_Pipeline(rhi.pipeline_cache, desc);
}
