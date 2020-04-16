#include "stdafx.h"
#include "graphics/rhi/vulkan/shader_access.h"

/*
void make_ShaderAcess() {
	ShaderModules* module = get_shader_config(assets, desc.shader, permutations[perm]);

	DescriptorSetInfo& material_bindings = module->info.sets[MATERIAL_SET];

	array<MAX_BINDING, VkDescriptorSetLayoutBinding> layout_bindings(material_bindings.bindings.length);

	DescriptorBindingInfo material_ubo_binding = {};

	for (uint i = 0; i < material_bindings.bindings.length; i++) {
		DescriptorBindingInfo& binding = material_bindings.bindings[i];

		VkDescriptorSetLayoutBinding& layout_binding = layout_bindings[binding.binding];
		layout_binding.binding = binding.binding;
		layout_binding.descriptorCount = binding.count;
		layout_binding.descriptorType = binding.type;
		layout_binding.stageFlags = binding.stage;

		if (binding.type_name == "Material") material_ubo_binding = binding;
	}

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = material_bindings.bindings.length;
	layout_info.pBindings = layout_bindings.data;

	VkDescriptorSetLayout layout;
	vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &layout);

	VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT] = { layout, layout, layout };

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptor_pool;
	allocInfo.descriptorSetCount = frames_in_flight;
	allocInfo.pSetLayouts = layouts;

	VkDescriptorSet descriptor_sets[MAX_FRAMES_IN_FLIGHT];

	if (vkAllocateDescriptorSets(device, &allocInfo, descriptor_sets) != VK_SUCCESS) {
		throw "Failed to make descriptor sets!";
	}

	for (int i = 0; i < frames_in_flight; i++) {


		make_Buffer(device, physical_device, )

			for (ParamDesc& param : desc.params) {

			}

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
*/