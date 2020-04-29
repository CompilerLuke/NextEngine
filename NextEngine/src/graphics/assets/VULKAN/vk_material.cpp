#include "stdafx.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/material.h"
#include "graphics/rhi/vulkan/shader.h"
#include "graphics/rhi/vulkan/texture.h"
#include "graphics/rhi/vulkan/buffer.h"
#include "graphics/assets/assets.h"
#include <stdio.h>

const int MATERIAL_SET = 1;

//todo move most of the code into helper functions
//as the material system isn't the most materials general representation of shader access
//Move many small buffers into one large one
//avoid doing many map and unmaps for performance reasons
//

Material make_material_descriptors(RHI& rhi, Assets& assets, MaterialDesc& desc) {
	Material material;
	material.desc = desc;


	uint frames_in_flight = get_frames_in_flight(rhi);
	VkDevice device = get_Device(rhi);
	VkPhysicalDevice physical_device = get_PhysicalDevice(rhi);
	VkDescriptorPool descriptor_pool = get_DescriptorPool(rhi);

	shader_flags permutations[] = { SHADER_INSTANCED };

	//todo allow multiple shader permutations

	static texture_handle white_texture = load_Texture(assets, "solid_white.png");

	for (int perm = 0; perm < 1; perm++) {
		ShaderModules* module = get_shader_config(assets, desc.shader, permutations[perm]);

		if (module->info.sets.length < 2) {
			ShaderInfo* info = shader_info(assets, desc.shader);
			fprintf(stderr, "Shader %s %s, does not support this material setup!\n", info->vfilename, info->ffilename);
			abort();
		}
		DescriptorSetInfo& material_bindings = module->info.sets[MATERIAL_SET];
		
		VkDescriptorSetLayoutBinding layout_bindings[MAX_BINDING] = {};

		for (uint i = 0; i < material_bindings.bindings.length; i++) {
			DescriptorBindingInfo& binding = material_bindings.bindings[i];
			
			VkDescriptorSetLayoutBinding& layout_binding = layout_bindings[i];
			layout_binding.binding = binding.binding;
			layout_binding.descriptorCount = binding.count;
			layout_binding.descriptorType = binding.type;
			layout_binding.stageFlags = binding.stage;
		}

		VkDescriptorSetLayoutCreateInfo layout_info = {};
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = material_bindings.bindings.length;
		layout_info.pBindings = layout_bindings;

		VkDescriptorSetLayout layout;
		vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &layout);

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptor_pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout;

		VkDescriptorSet descriptor_set;

		if (vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set) != VK_SUCCESS) {
			throw "Failed to make descriptor sets!";
		}

		material.set = descriptor_set;
		material.layout = layout;

		UBOInfo& ubo_info = material_bindings.ubos[0];
		assert(ubo_info.type_name == "Material");

		make_Buffer(device, physical_device, ubo_info.size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, material.ubo, material.memory);

		array<MAX_SAMPLERS + 1, VkWriteDescriptorSet> descriptor_writes = {};
		descriptor_writes.resize(material_bindings.samplers.length + 1);

		VkDescriptorBufferInfo ubo_buffer_info = {};
		ubo_buffer_info.buffer = material.ubo;
		ubo_buffer_info.offset = 0;
		ubo_buffer_info.range = ubo_info.size;
			
		VkWriteDescriptorSet& ubo_set = descriptor_writes[0];
		ubo_set = {};
		ubo_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ubo_set.dstSet = descriptor_set;
		ubo_set.dstBinding = 0;
		ubo_set.dstArrayElement = 0;
		ubo_set.descriptorCount = 1;
		ubo_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_set.pBufferInfo = &ubo_buffer_info;

		VkDescriptorImageInfo image_infos[MAX_SAMPLERS] = {};
		char ubo_memory[256] = {};

		uint sampler_index = 0;
		uint uniform_index = 0;
			
		for (ParamDesc& param : desc.params) {
			bool is_ubo_field = param.type != Param_Image;
			bool is_channel = param.type == Param_Channel1 || param.type == Param_Channel2 || param.type == Param_Channel3;
			bool is_sampler = param.type == Param_Image || is_channel;
			
			if (is_sampler) {
				SamplerInfo& sampler_info = material_bindings.samplers[sampler_index];
				VkSampler sampler = make_TextureSampler(device);

				DescriptorBindingInfo& binding_info = material_bindings.bindings[sampler_info.id];
					
				if (binding_info.name.length() != 0 && binding_info.name != param.name) {
					fprintf(stderr, "Material Param Description matters.\n");
					fprintf(stderr, "Description specifies %s, but shader declares %s\n", binding_info.name.data, param.name);
					abort();
				}

				texture_handle handle = { param.image };
				if (handle.id == INVALID_HANDLE) handle = white_texture;

				VkDescriptorImageInfo* image_info = &image_infos[sampler_index++];
				image_info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				image_info->imageView = get_Texture(assets, handle)->view;
				image_info->sampler = sampler;

				//NOTE: sampler index is +1 as 0 is the uniform buffer
				VkWriteDescriptorSet& descriptor = descriptor_writes[sampler_index];
				descriptor = {};
				descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptor.dstSet = descriptor_set;
				descriptor.descriptorCount = 1;
				descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptor.dstBinding = binding_info.binding;
				descriptor.pImageInfo = image_info;

				material.samplers.append(sampler);
			}

			if (is_ubo_field) {
				UBOFieldInfo& ubo_field_info = ubo_info.fields[uniform_index++];
					
				char* field_ptr = (char*)ubo_memory + ubo_field_info.offset;

				char name[100];
				if (is_channel) sprintf_s(name, "%s_scalar", param.name);
				else sprintf_s(name, "%s", param.name);

				printf("UBO FIELD %s, param %s %i\n", ubo_field_info.name.data, name, ubo_field_info.offset);
				if (ubo_field_info.name != name) {
					printf("Material Description and Shader Layout do not match!");
					abort();
				}

				if (param.type == Param_Int) {
					*(int*)field_ptr = param.integer;
				}

				if (param.type == Param_Float || param.type == Param_Channel1) {
					*(float*)field_ptr = param.real;
				}

				if (param.type == Param_Vec2 || param.type == Param_Channel2) {
					*(glm::vec2*)field_ptr = param.vec2;
				}

				if (param.type == Param_Vec3 || param.type == Param_Channel3) {
					*(glm::vec3*)field_ptr = param.vec3;
				}
			}
		}


		if (material_bindings.samplers.length != sampler_index) {
			fprintf(stderr, "Mismatch in the number of image parameters!\n");
			abort();
		}

		if (ubo_info.fields.length != uniform_index) {
			fprintf(stderr, "Mismatch in the number of ubo fields!\n");
			abort();
		}

		memcpy_Buffer(device, material.memory, ubo_memory, ubo_info.size);

		vkUpdateDescriptorSets(device, descriptor_writes.length, descriptor_writes.data, 0, nullptr);
	}

	return material;
}