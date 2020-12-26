#include "graphics/rhi/rhi.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/shader_access.h"
#include "graphics/rhi/shader_access.h"
#include "graphics/assets/texture.h"
#include "graphics/rhi/vulkan/texture.h"
#include "graphics/assets/assets.h"
#include "core/container/tvector.h"

void add_combined_sampler(DescriptorDesc& desc, Stage stage, slice<CombinedSampler> combined_sampler, uint binding_address) {
	DescriptorDesc::Binding binding;
	binding.binding = binding_address;
	binding.type = DescriptorDesc::COMBINED_SAMPLER;
	binding.stage = stage;
	binding.samplers = combined_sampler;

	desc.bindings.append(binding);
}

void add_combined_sampler(DescriptorDesc& desc, Stage stage, sampler_handle sampler, texture_handle texture, uint binding_address) {
	CombinedSampler& combined_sampler = *TEMPORARY_ALLOC(CombinedSampler);
	combined_sampler = {};
	combined_sampler.sampler = sampler;
	combined_sampler.texture = texture;
	
	add_combined_sampler(desc, stage, combined_sampler, binding_address);
}

void add_combined_sampler(DescriptorDesc& desc, Stage stage, sampler_handle sampler, cubemap_handle cubemap, uint binding_address) {
	CombinedSampler& combined_sampler = *TEMPORARY_ALLOC(CombinedSampler);
	combined_sampler = {};
	combined_sampler.sampler = sampler;
	combined_sampler.cubemap = cubemap;

	add_combined_sampler(desc, stage, combined_sampler, binding_address);
}

void add_ubo(DescriptorDesc& desc, Stage stage, slice<UBOBuffer> ubo_buffer, uint binding_address) {
	DescriptorDesc::Binding binding;
	binding.binding = binding_address;
	binding.ubos = ubo_buffer;
	binding.type = DescriptorDesc::UBO_BUFFER;
	binding.stage = stage;

	desc.bindings.append(binding);
}

VkDescriptorSet make_DescriptorSet(VkDevice device, VkDescriptorPool pool,  slice<VkDescriptorSetLayout> set_layouts) {
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = set_layouts.length;
	allocInfo.pSetLayouts = set_layouts.data;

	VkDescriptorSet descriptor_set;

	if (vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set) != VK_SUCCESS) {
		throw "Failed to make descriptor sets!";
	}

	return descriptor_set;
}

VkDescriptorSetLayout make_set_layout(VkDevice device, DescriptorSetInfo& info) {
	array<MAX_BINDING, VkDescriptorSetLayoutBinding> layout_bindings;
	slice<DescriptorBindingInfo> binding_info = info.bindings;

	assert(binding_info.length <= MAX_BINDING);

	for (uint i = 0; i < binding_info.length; i++) {
		DescriptorBindingInfo& binding = binding_info[i];
		if (binding.count == 0) continue;

		VkDescriptorSetLayoutBinding layout_binding = {};
		layout_binding.binding = binding.binding;
		layout_binding.descriptorCount = binding.count;
		layout_binding.descriptorType = binding.type;
		layout_binding.stageFlags = binding.stage;			

		layout_bindings.append(layout_binding);
	}

	return make_set_layout(device, layout_bindings);
}

void update_descriptor_set(VkDevice device, VkDescriptorSet descriptor_set, DescriptorBinding& binding) {	
	array<MAX_SAMPLERS + MAX_UBOS, VkWriteDescriptorSet> descriptor_writes = {};

	VkDescriptorBufferInfo ubo_buffer_infos[MAX_UBOS] = {};
	VkDescriptorImageInfo sampler_infos[MAX_SAMPLERS] = {};

	for (DescriptorBinding::BufferInfo& buffer : binding.ubos) {		
		//for (DescriptorBinding::BufferInfo info : binding.ubos[i].binding) {

		VkWriteDescriptorSet ubo_set = {};
		ubo_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ubo_set.dstSet = descriptor_set;
		ubo_set.dstBinding = buffer.binding;
		ubo_set.dstArrayElement = 0;
		ubo_set.descriptorCount = buffer.info.length;
		ubo_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_set.pBufferInfo = buffer.info.data;

		descriptor_writes.append(ubo_set);
	}

	for (DescriptorBinding::ImageInfo& image : binding.samplers) {
		VkWriteDescriptorSet descriptor = {};
		descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor.dstSet = descriptor_set;
		descriptor.descriptorCount = image.info.length;
		descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor.dstBinding = image.binding;
		descriptor.pImageInfo = image.info.data;

		descriptor_writes.append(descriptor);
	}

	vkUpdateDescriptorSets(device, descriptor_writes.length, descriptor_writes.data, 0, nullptr);
}

struct Descriptors {
	uint count;
	VkDescriptorSetLayout layouts[100];
	VkDescriptorSet sets[100];

	array<100, descriptor_set_handle> free_handles;
};

static Descriptors descriptors;

VkShaderStageFlags to_vk_stage(Stage stage) {
	VkShaderStageFlags result = 0;

	if (stage & VERTEX_STAGE) result |= VK_SHADER_STAGE_VERTEX_BIT;
	if (stage & FRAGMENT_STAGE) result |= VK_SHADER_STAGE_FRAGMENT_BIT;

	return result;
}

VkDescriptorSetLayout make_descriptor_set_layout(DescriptorDesc& desc) {
	array<MAX_BINDING, VkDescriptorSetLayoutBinding> layout_bindings;

	for (DescriptorDesc::Binding& binding : desc.bindings) {
		VkDescriptorType types[] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };

		VkDescriptorSetLayoutBinding layout = {};
		layout.descriptorCount = binding.ubos.length;
		layout.descriptorType = types[binding.type];
		layout.binding = binding.binding;
		layout.stageFlags = to_vk_stage(binding.stage);

		layout_bindings.append(layout);
	}

	return make_set_layout(rhi.device, layout_bindings);
}

void update_descriptor_set(descriptor_set_handle& handle, DescriptorDesc& desc) {	
	array<MAX_BINDING, DescriptorBinding::BufferInfo> buffer_infos;
	array<MAX_BINDING, DescriptorBinding::ImageInfo> image_infos;

	for (DescriptorDesc::Binding& binding : desc.bindings) {
		if (binding.type == DescriptorDesc::UBO_BUFFER) {
			VkDescriptorBufferInfo* descriptor_array = TEMPORARY_ARRAY(VkDescriptorBufferInfo, binding.ubos.length);
			
			for (uint i = 0; i < binding.ubos.length; i++) {
				VkDescriptorBufferInfo& info = descriptor_array[i];
				
				info = {};
				info.buffer = rhi.ubo_allocator.ubo_memory.buffer;
				info.offset = binding.ubos[i].offset;
				info.range = binding.ubos[i].size;
			}

			buffer_infos.append({ binding.binding, {descriptor_array, binding.ubos.length}});
		}

		if (binding.type == DescriptorDesc::COMBINED_SAMPLER) {
			VkDescriptorImageInfo* descriptor_array = TEMPORARY_ARRAY(VkDescriptorImageInfo, binding.samplers.length);
			
			for (uint i = 0; i < binding.samplers.length; i++) {
				VkDescriptorImageInfo& info = descriptor_array[i];

				VkImageView view = VK_NULL_HANDLE;

				CombinedSampler& combined = binding.samplers[i];

				if (combined.texture.id != INVALID_HANDLE) view = get_Texture(combined.texture)->view;
				if (combined.cubemap.id != INVALID_HANDLE) view = get_Cubemap(combined.cubemap)->view;

				assert(view != VK_NULL_HANDLE);

				info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				info.imageView = view;
				info.sampler = get_Sampler(binding.samplers[i].sampler);
			}

			image_infos.append({ binding.binding, {descriptor_array, binding.samplers.length}});
		}
	}

	VkDescriptorSetLayout layout;
	VkDescriptorSet descriptor_set;

	if (handle.id == INVALID_HANDLE) {
		layout = make_descriptor_set_layout(desc); 
		//todo layouts should either be cached or created seperately
		
		uint offset = descriptors.free_handles.length > 0 ? 
			descriptors.free_handles.pop().id - 1 
			: descriptors.count++;
		
		descriptor_set = make_set(rhi.descriptor_pool, layout);

		descriptors.layouts[offset] = layout;
		descriptors.sets[offset] = descriptor_set;

		handle.id = offset + 1;
	}
	else {
		layout = get_descriptor_set_layout(handle);
		descriptor_set = get_descriptor_set(handle);
	}

	DescriptorBinding binding;
	binding.ubos = buffer_infos;
	binding.samplers = image_infos;

	update_descriptor_set(rhi.device, descriptor_set, binding);


}

void destroy_descriptor_set(descriptor_set_handle handle) {
	if (handle.id == INVALID_HANDLE) return;

	VkDescriptorSetLayout layout = get_descriptor_set_layout(handle);
	VkDescriptorSet set = get_descriptor_set(handle);

	vkDestroyDescriptorSetLayout(rhi.device, layout, nullptr);
	vkFreeDescriptorSets(rhi.device, rhi.descriptor_pool, 1, &set);
	
	descriptors.free_handles.append(handle);
}

VkDescriptorSet get_descriptor_set(descriptor_set_handle set) {
	if (set.id == INVALID_HANDLE) return nullptr;
	return descriptors.sets[set.id - 1];
}

VkDescriptorSetLayout get_descriptor_set_layout(descriptor_set_handle set) {
	return descriptors.layouts[set.id - 1];
}

void make_DescriptorPool(DescriptorPool& pool, VkDevice device, VkPhysicalDevice physical_device, const DescriptorCount& count) {
	pool.device = device;
	pool.physical_device = physical_device;
	
	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = count.max_ubos;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = count.max_samplers;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = count.max_sets;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; 
	//todo it might be worth creating several pools
	//since this flag may be expensive, as it can no longer be linearly allocated!

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool.pool) != VK_SUCCESS) {
		throw "Failed to make descriptor pool!";
	}
}

void destroy_DescriptorPool(DescriptorPool& pool) {
	vkDestroyDescriptorPool(pool.device, pool.pool, nullptr);
}

VkDescriptorSetLayout make_set_layout(VkDevice device, slice<VkDescriptorSetLayoutBinding> bindings) {
	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = bindings.length;
	layout_info.pBindings = bindings.data;

	VkDescriptorSetLayout layout;
	vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &layout);
	return layout;
}

//this function is pointless
void destroy_SetLayout(VkDevice device, VkDescriptorSetLayout layout) {
	vkDestroyDescriptorSetLayout(device, layout, nullptr);
}

VkDescriptorSet make_set(DescriptorPool& pool, slice<VkDescriptorSetLayout> set_layouts) {
	return make_DescriptorSet(pool.device, pool, set_layouts);
}

void recycle_descriptor_set(periodically_updated_descriptor& set) {
	//printf("CURRENT DESCRIPTOR %i\n", set.current.id);
	/*
	uint frame = rhi.frame_index;

	descriptor_set_handle& handle = set.modified_in_frame[frame];
	assert(handle.id != set.current.id);
	//printf("RECYCLING DESCRIPTOR %i\n", handle.id);
	destroy_descriptor_set(handle);


	handle = { INVALID_HANDLE };
	*/
}

void update_descriptor_set(periodically_updated_descriptor& set, DescriptorDesc& desc) {
	uint frame = rhi.frame_index;

	set.modified_in_frame[set.updated_in_frame] = set.current;
	
	if (set.current.id) {
		queue_for_destruction((void*)(size_t)set.current.id, [](void* ptr) {
			descriptor_set_handle handle{ (uint)(size_t)ptr };
			destroy_descriptor_set(handle);
		});
	}

	set.current = { INVALID_HANDLE };
	update_descriptor_set(set.current, desc);

	set.updated_in_frame = frame;
}
